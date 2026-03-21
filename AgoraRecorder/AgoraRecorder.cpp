#include <iostream>
#include <string>
#include <IAgoraRtcEngine.h>
#include <AgoraRefPtr.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <windows.h>
#include <direct.h>  // For _mkdir
#include <cerrno>   // For checking if folder already exists

#include "MyBase64.h"
#include "Utils.h"

const std::string VIDEO_NAME = "video.h264"; 
const std::string VIDEOTS_NAME = "video_ts.txt"; 
const std::string AUDIO_NAME = "audio.pcm"; 

using namespace agora::rtc;
using namespace agora::media;
using namespace utils;

// A simple shared state structure
struct RecordingContext
{
	std::chrono::steady_clock::time_point recordingStartTime;
	std::atomic<std::chrono::steady_clock::time_point> lastActivityTime{ std::chrono::steady_clock::now() };
	std::string folder;
};

void WaitInput()
{
	std::cout << "\nPress Enter to exit..." << std::endl;

	// Clear any characters left in the input buffer
	std::cin.clear();
	std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

	// Wait for the user to press Enter
	std::cin.get();
}

struct AgoraConfig
{
	std::string appId;
	std::string channel;
	std::string token;
	std::string userId;
	std::string encryptionKey;
	std::string encryptionSalt;
	bool viewer = false;
	bool log = true;

	AgoraConfig() = default;

	// Helper to trim whitespace from file lines
	std::string trim(const std::string& str)
	{
		size_t first = str.find_first_not_of(" \t\r\n");
		if (std::string::npos == first) return "";
		size_t last = str.find_last_not_of(" \t\r\n");
		return str.substr(first, (last - first + 1));
	}

	// Logic shared by both CLI and File parsing
	void setProperty(std::string key, std::string val)
	{
		// Strip leading dashes if coming from CLI (e.g., -app_id -> app_id)
		if (!key.empty() && key[0] == '-')
		{
			key = key.substr(1);
		}

		if (key == "app_id") appId = val;
		else if (key == "channel_name") channel = val;
		else if (key == "token") token = val;
		else if (key == "user_id") userId = val;
		else if (key == "encryption_key") encryptionKey = val;
		else if (key == "encryption_salt") encryptionSalt = val;
	}

	// Load from a .txt or .ini file
	bool loadFromFile(const std::string& filename)
	{
		std::ifstream file(filename);
		if (!file.is_open())
		{
			std::cout << "Did not find file " << filename << std::endl;
			return false;
		}

		std::cout << "Loaded config from " << filename << std::endl;

		std::string line;
		while (std::getline(file, line))
		{
			line = trim(line);

			// Skip comments and empty lines
			if (line.empty() || line[0] == '#' || line[0] == ';') continue;

			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				std::string key = trim(line.substr(0, pos));
				std::string val = trim(line.substr(pos + 1));
				setProperty(key, val);
			}
		}
		return true;
	}

	// Updated Constructor
	AgoraConfig(int argc, char* argv[])
	{
		// 1. Optional: Load defaults from a file first if it exists
		if (!loadFromFile("config.ini") && argc == 1)
		{
			printHelp();
			WaitInput();
			std::exit(0);
		}

		// 2. Overwrite with command line arguments (CLI takes priority)
		for (int i = 1; i < argc; ++i)
		{
			std::string arg = argv[i];

			// Check for help flags
			if (arg == "-h" || arg == "-help" || arg == "--help")
			{
				printHelp();
				WaitInput();
				std::exit(0);
			}

			if (arg == "-viewer")
			{
				viewer = true;
			}

			if (arg == "-nolog")
			{
				log = false;
			}

			// Handle the case where the user passes a config file path via CLI
			if (arg.find("-config=") == 0)
			{
				loadFromFile(arg.substr(8));
				continue;
			}

			size_t pos = arg.find('=');
			if (pos != std::string::npos)
			{
				setProperty(arg.substr(0, pos), arg.substr(pos + 1));
			}
		}
	}

	void logConfig()
	{
		std::cout << "\n[APPLICATION CONFIGURATION]" << std::endl;
		std::cout << "  App ID:          " << appId << std::endl;
		std::cout << "  Channel:         " << channel << std::endl;
		std::cout << "  User ID (uid):   " << userId << std::endl;

		// Masking sensitive data
		std::cout << "  Token:           " << (token.empty() ? "EMPTY" : token) << std::endl;
		std::cout << "  Encryption Key:  " << (encryptionKey.empty() ? "DISABLED" : encryptionKey) << std::endl;
		std::cout << "  Encryption Salt: " << (encryptionSalt.empty() ? "NONE" : encryptionSalt) << std::endl;
		std::cout << "------------------------------------------\n" << std::endl;
	}

	void printHelp()
	{
		std::cout << "\nUsage: ./AgoraRecorder [options]\n\n"
			<< "Options:\n"
			<< "  -app_id=<id>           The Agora App ID\n"
			<< "  -channel_name=<name>   The channel name\n"
			<< "  -token=<token>         (Optional) The RTC token\n"
			<< "  -user_id=<id>          (Optional) The User ID (as string)\n"
			<< "  -encryption_key=<key>  (Optional) Encryption key for media\n"
			<< "  -encryption_salt=<salt> (Optional) Base64 encoded salt\n"
			<< "  -config=<path>         Or load these settings from an ini file\n"
			<< "  -help, --help, -h      Show this help menu\n\n"
			<< "Notes:\n"
			<< "  - CLI arguments take priority over values in config.ini.\n"
			<< "  - Config file format: key=value (one per line, # for comments)\n" << std::endl;
	}
};


class MyEncodedFrameObserver : public agora::media::IVideoEncodedFrameObserver
{
public:
	AgoraConfig agoraConfig;
	std::ofstream infoFile;
	std::ofstream videotsFile;
	std::ofstream outputFile;
	bool isHeaderFound = false;
	RecordingContext* Ctx = nullptr;
	bool isActive = true;
	uint64_t firstVideoTS;

	MyEncodedFrameObserver() = default;

	~MyEncodedFrameObserver() override
	{
		Cleanup();
	}

	void SetSharedCtx(RecordingContext* InCtx)
	{
		Ctx = InCtx;
	}

	bool onEncodedVideoFrameReceived(const char* channelId,
									uid_t uid,
									const uint8_t* imageBuffer,
									size_t length,
									const EncodedVideoFrameInfo& videoEncodedFrameInfo) override
	{
		auto now = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - Ctx->recordingStartTime);
		auto monoMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		long long localMs;
		// 1. If we haven't found a keyframe, check this frame
		if (!isHeaderFound)
		{
			// if (videoEncodedFrameInfo.frameType == agora::rtc::VIDEO_FRAME_TYPE_KEY_FRAME)
			{
				Ctx->recordingStartTime = now;
				localMs = 0;

				std::string videoPath = Ctx->folder + "\\" + VIDEO_NAME;
				std::string infoPath = Ctx->folder + "\\fullVideoInfo.csv";
				std::string videotsPath = Ctx->folder + "\\" + VIDEOTS_NAME;

				infoFile.open(infoPath.c_str(), std::ios::out);
				videotsFile.open(videotsPath.c_str(), std::ios::out);
				videotsFile << "# timestamp format v2\n";

				printf("Keyframe found! Opening file and starting recording...\n");

				isHeaderFound = true;
				outputFile.open(videoPath, std::ios::binary | std::ios::out);
				firstVideoTS = monoMs;
				std::cout << "First video frame: " << monoMs << std::endl;
			}
		}
		else
		{
			// std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
			localMs = delta.count();
		}

		if (infoFile.is_open())
		{
			// For every frame (including the first one), calculate the local offset
			infoFile
				<< monoMs << ","
				<< localMs << ","
				<< videoEncodedFrameInfo.captureTimeMs << ","
				<< videoEncodedFrameInfo.presentationMs << ","
				<< videoEncodedFrameInfo.decodeTimeMs << ","
				<< getCodecTypeString(videoEncodedFrameInfo.codecType) << ","
				<< videoEncodedFrameInfo.width << ","
				<< videoEncodedFrameInfo.height << ","
				<< videoEncodedFrameInfo.framesPerSecond << ","
				<< videoEncodedFrameInfo.rotation << ","
				<< videoEncodedFrameInfo.trackId << ","
				<< videoEncodedFrameInfo.frameType << ","
				<< videoEncodedFrameInfo.streamType << ","
				<< length << "\n";
		}

		// 3. If the file is open, write the data
		if (outputFile.is_open())
		{
			Ctx->lastActivityTime = now;

			outputFile.write(reinterpret_cast<const char*>(imageBuffer), static_cast<long>(length));
			outputFile.flush(); // Ensure data is written to disk
			infoFile.flush();
		}

		if (videotsFile.is_open())
		{
			videotsFile << localMs << "\n";
		}

		return true;
	}

	void Cleanup()
	{
		if (outputFile.is_open())
		{
			outputFile.close();
		}

		if (infoFile.is_open())
		{
			infoFile.close();
		}

		if (videotsFile.is_open())
		{
			videotsFile.close();
		}
	}
};

class MyRawAudioObserver : public agora::media::IAudioFrameObserver
{
public:
	MyRawAudioObserver() = default;

	~MyRawAudioObserver() override
	{
		Cleanup();
	}

	AgoraConfig agoraConfig;
	RecordingContext* Ctx = nullptr;
	bool isActive = true;
	std::ofstream infoFile;
	std::ofstream outputFile;
	bool isStarted = false;
	uint64_t firstAudioTS;

	void SetSharedCtx(RecordingContext* InCtx)
	{
		Ctx = InCtx;
	}

	// 1. Tell Agora which callbacks to trigger.
	// We want PLAYBACK (what we hear).
	virtual int getObservedAudioFramePosition() override
	{
		return agora::media::IAudioFrameObserverBase::AUDIO_FRAME_POSITION_PLAYBACK;
	}

	// 2. Set the format you want to receive (48kHz, Stereo, 1024 samples per call)
	virtual AudioParams getPlaybackAudioParams() override
	{
		// AudioParams(sampleRate, channels, mode, samplesPerCall)
		// Mode: RAW_AUDIO_FRAME_OP_MODE_READ_ONLY (since we are just recording)
		return agora::media::IAudioFrameObserverBase::AudioParams(48000, 2, agora::rtc::RAW_AUDIO_FRAME_OP_MODE_READ_ONLY, 1024);
	}

	// 3. Receive the raw PCM data
	virtual bool onPlaybackAudioFrame(const char* channelId, AudioFrame& audioFrame) override
	{
		auto now = std::chrono::steady_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - Ctx->recordingStartTime);
		auto monoMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// 1. DUMMY FRAME CHECK
		// If timestamps are 0, this is "empty" data. Stop processing here.
		if (audioFrame.rtpTimestamp == 0)
		{
			return true;
		}

		if (!isStarted)
		{
			std::string videoPath = Ctx->folder + "\\" + AUDIO_NAME;
			std::string infoPath = Ctx->folder + "\\fullAudioInfo.csv";
			outputFile.open(videoPath.c_str(), std::ios::binary | std::ios::out);
			infoFile.open(infoPath.c_str(), std::ios::out);
			isStarted = true;

			firstAudioTS = monoMs;
			std::cout << "First audio frame: " << monoMs << std::endl;
		}

		if (infoFile.is_open())
		{
			auto localMs = delta.count();

			// Log to CSV (Matching the struct field names)
			infoFile
				<< monoMs << ","
				<< localMs << ","
				<< audioFrame.renderTimeMs << ","
				<< audioFrame.presentationMs << ","
				<< audioFrame.samplesPerSec << ","
				<< audioFrame.channels << ","
				<< audioFrame.samplesPerChannel << ","
				<< (int)audioFrame.bytesPerSample << ","
				<< audioFrame.rtpTimestamp << "\n";
		}

		if (outputFile.is_open())
		{
			Ctx->lastActivityTime = now;

			// Calculate size: samples * channels * 2 (because 16-bit PCM is 2 bytes per sample)
			size_t bytesToWrite = audioFrame.samplesPerChannel * audioFrame.channels * 2;
			outputFile.write(reinterpret_cast<const char*>(audioFrame.buffer), bytesToWrite);
		}

		return true;
	}

	void Cleanup()
	{
		if (outputFile.is_open())
		{
			outputFile.close();
		}

		if (infoFile.is_open())
		{
			infoFile.close();
		}
	}

	// Required overrides (mostly return true/default)
	virtual bool onRecordAudioFrame(const char* channelId, AudioFrame& audioFrame) override { return true; }
	virtual bool onMixedAudioFrame(const char* channelId, AudioFrame& audioFrame) override { return true; }
	virtual bool onPlaybackAudioFrameBeforeMixing(const char* channelId, agora::rtc::uid_t uid, AudioFrame& audioFrame) override { return true; }
	virtual bool onPlaybackAudioFrameBeforeMixing(const char* channelId, const char* userId, AudioFrame& audioFrame) override { return true; }
	virtual bool onEarMonitoringAudioFrame(AudioFrame& audioFrame) override { return true; }

	// Use default params for others
	virtual AudioParams getRecordAudioParams() override { return AudioParams(); }
	virtual AudioParams getMixedAudioParams() override { return AudioParams(); }
	virtual AudioParams getEarMonitoringAudioParams() override { return AudioParams(); }
};

class MyFrameObserver : public agora::media::IVideoFrameObserver
{
public:
	RecordingContext* Ctx = nullptr;
	std::ofstream infoFile;

	MyFrameObserver() = default;

	bool onRenderVideoFrame(const char* channelId, uid_t remoteUid, VideoFrame& videoFrame) override
	{
		auto monoMs = getMonotonicMs();

		if (!infoFile.is_open())
		{
			std::string infoPath = Ctx->folder + "\\videoRenderTime.csv";
			if (!infoFile.is_open())
			{
				infoFile.open(infoPath.c_str(), std::ios::out);
			}
		}

		if (infoFile.is_open())
		{
			// For every frame (including the first one), calculate the local offset
			infoFile << videoFrame.renderTimeMs << "," << monoMs << "\n";
			infoFile.flush();
		}
		return true;
	}

	void Cleanup()
	{
		if (infoFile.is_open())
		{
			infoFile.close();
		}
	}

	bool onCaptureVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame) override { return true; }
	bool onPreEncodeVideoFrame(agora::rtc::VIDEO_SOURCE_TYPE sourceType, VideoFrame& videoFrame) override { return true; }
	bool onMediaPlayerVideoFrame(VideoFrame& videoFrame, int mediaPlayerId) override { return true; }
	bool onTranscodedVideoFrame(VideoFrame& videoFrame) override { return true; }
};


class MyEventHandler : public IRtcEngineEventHandler
{
public:
	IRtcEngine* engine;
	AgoraConfig agoraConfig;
	HWND videoHwnd;
	RecordingContext* Ctx = nullptr;
	std::atomic<bool> UserLeft{false};

	void onUserJoined(uid_t uid, int elapsed) override
	{
		std::cout << "[EVENT] User joined: " << uid << std::endl;

		if (agoraConfig.viewer && videoHwnd)
		{
			VideoCanvas canvas;
			canvas.view = videoHwnd; // The HWND created in main
			canvas.uid = uid; // The UID of the person who just joined
			canvas.renderMode = agora::media::base::RENDER_MODE_FIT;

			int ret = engine->setupRemoteVideo(canvas);
			if (ret != 0)
			{
				std::cerr << "  SetupRemoteVideo failed: " << getAgoraError(ret) << std::endl;
				return;
			}

			std::cout << "  Video canvas bound to UID: " << uid << std::endl;
		}

		if (UserLeft)
		{
			std::cout << "  User rejoined the stream!" << uid << std::endl;
		}

		UserLeft = false;
	}

	void onUserOffline(uid_t uid, USER_OFFLINE_REASON_TYPE reason) override
	{
		std::cout << "[EVENT] User Offline" << std::endl;
		std::cout << "  UID: " << uid << std::endl;
		std::cout << "  Reason: " << OfflineReasonToString(reason) << " (" << reason << ")" << std::endl;

		if (videoHwnd)
		{
			// 2. Force the window to redraw itself (which will trigger WM_PAINT)
			// This clears the "frozen" last frame.
			InvalidateRect(videoHwnd, nullptr, true);
			UpdateWindow(videoHwnd);
		}

		UserLeft = true;
		std::cout << "Host left, waiting to check if they come back..." << std::endl;
		// PostMessage(videoHwnd, WM_CLOSE, 0, 0); 
	}

	void onLeaveChannel(const RtcStats& stats) override
	{
		std::cout << "[EVENT] Left Channel" << std::endl;
		std::cout << "  Duration: " << stats.duration << "s" << std::endl;
		std::cout << "  Total Received: " << (stats.rxBytes / 1024) << " KB" << std::endl;
		std::cout << "  Total Sent: " << (stats.txBytes / 1024) << " KB" << std::endl;
		std::cout << "  Avg RX Bitrate: " << stats.rxAudioKBitRate + stats.rxVideoKBitRate << " kbps" << std::endl;
		std::cout << "  CPU Usage (App/Total): " << stats.cpuAppUsage << "% / " << stats.cpuTotalUsage << "%" << std::endl;

		UserLeft = true;
	}

	void onError(int err, const char* msg) override
	{
		AgoraErrorInfo errorInfo = getAgoraErrorInfo(err);
		std::cerr << "[EVENT] " << errorInfo.toString() << std::endl;
	}
};

// A basic window procedure to handle the "X" button and drawing
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND CreateVideoWindow(int width, int height)
{
	const wchar_t CLASS_NAME[] = L"AgoraVideoWindow";
	HINSTANCE hInstance = GetModuleHandle(NULL);

	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Agora Video Stream",
								WS_OVERLAPPEDWINDOW | WS_VISIBLE,
								CW_USEDEFAULT, CW_USEDEFAULT, width, height,
								NULL, NULL, hInstance, NULL);

	return hwnd;
}

namespace
{
	MyEventHandler myEventHandler;
	MyEncodedFrameObserver videoObserver;
	MyRawAudioObserver audioObserver;

	MyFrameObserver rawFrameObserver;
}

int main(int argc, char* argv[])
{
	std::cout << "Start AgoraRecorder" << std::endl;
	AgoraConfig agoraConfig(argc, argv);
	agoraConfig.logConfig();

	// // 2. Initialize Agora Engine
	IRtcEngine* engine = createAgoraRtcEngine();
	if (!engine)
	{
		std::cerr << "FAILED: createAgoraRtcEngine nullptr " << std::endl;
		WaitInput();
		return -1;
	}

	// 1. Create the window
	if (agoraConfig.viewer)
	{
		HWND videoHwnd = CreateVideoWindow(800, 600);
		myEventHandler.videoHwnd = videoHwnd;
	}
	else
	{
		std::cout << "Skip making window?" << std::endl;
	}

	RecordingContext* ctx = new RecordingContext();
	myEventHandler.engine = engine;
	myEventHandler.agoraConfig = agoraConfig;
	myEventHandler.Ctx = ctx;

	RtcEngineContext context;
	context.appId = agoraConfig.appId.c_str();
	context.eventHandler = &myEventHandler;
	context.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;

	std::chrono::time_point<std::chrono::system_clock> startTime = std::chrono::system_clock::now();
	std::time_t now_time_t = std::chrono::system_clock::to_time_t(startTime);

	std::tm utc_tm = *std::gmtime(&now_time_t);

	std::ostringstream formattedTime;
	formattedTime << std::put_time(&utc_tm, "%Y%m%d_%H%M%S");

	std::string logPath;

	std::string folderName = formattedTime.str() + "_" + agoraConfig.channel;

	if (agoraConfig.log)
	{
		agora::commons::LogConfig logConfig;

		if (!directoryExists(folderName))
		{
			_mkdir(folderName.c_str());
		}

		logPath = folderName + "\\agora.log";

		std::cout << "Log path: " << logPath << std::endl;

		logConfig.filePath = logPath.c_str();
		logConfig.fileSizeInKB = 2048; // 2MB
		logConfig.level = agora::commons::LOG_LEVEL::LOG_LEVEL_INFO;
		context.logConfig = logConfig;
	}

	int ret = engine->initialize(context);
	if (ret != 0)
	{
		std::cerr << "FAILED: engine->initialize returned " << getAgoraError(ret) << std::endl;
		WaitInput();
		return -1;
	}

	std::cout << "Initialized Engine." << std::endl;

	// 2. Enable Video
	engine->enableVideo();
	engine->enableAudio();
	// engine->startPreview();

	ChannelMediaOptions options;
	// options.enableBuiltInMediaEncryption = true;
	options.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;
	options.clientRoleType = CLIENT_ROLE_AUDIENCE;
	options.autoSubscribeAudio = true;
	options.autoSubscribeVideo = true;
	if (!agoraConfig.token.empty())
	{
		options.token = agoraConfig.token.c_str();
	}

	if (!agoraConfig.viewer)
	{
		if (!directoryExists(folderName))
		{
			_mkdir(folderName.c_str());
		}

		agora::util::AutoPtr<agora::media::IMediaEngine> mediaEngine;
		// //query interface agora::AGORA_IID_MEDIA_ENGINE in the engine.
		mediaEngine.queryInterface(engine, agora::rtc::AGORA_IID_MEDIA_ENGINE);

		ctx->folder = folderName;
		videoObserver.SetSharedCtx(ctx);
		videoObserver.agoraConfig = agoraConfig;
		audioObserver.SetSharedCtx(ctx);
		audioObserver.agoraConfig = agoraConfig;

		rawFrameObserver.Ctx = ctx;
		if (mediaEngine->registerVideoFrameObserver(&rawFrameObserver) != 0)
		{
			std::cout << "Failed to register test observer" << std::endl;
		}

		mediaEngine->registerVideoEncodedFrameObserver(&videoObserver);
		mediaEngine->registerAudioFrameObserver(&audioObserver);
	}

	// encryption
	{
		if (!agoraConfig.encryptionKey.empty() && !agoraConfig.encryptionSalt.empty())
		{
			EncryptionConfig config;
			config.encryptionMode = AES_256_GCM2;
			config.encryptionKey = agoraConfig.encryptionKey.c_str();

			// salt
			{
				std::string saltStr = agoraConfig.encryptionSalt;

				// This removes \n, \r, and spaces from the string
				saltStr.erase(std::remove_if(saltStr.begin(), saltStr.end(), ::isspace), saltStr.end());

				// 1. Decode using the library
				std::string decodedStr = base64_decode(saltStr);

				// 2. Convert string to vector<uint8_t> (or copy directly)
				// Java's byte[] is equivalent to C++'s std::vector<uint8_t>
				std::vector<uint8_t> decoded(decodedStr.begin(), decodedStr.end());

				// Zero out the fixed 32-byte salt buffer first
				std::memset(config.encryptionKdfSalt, 0, 32);

				// Copy decoded bytes into the Agora struct (limit to 32 bytes)
				size_t copyLen = min((size_t)32, decoded.size());
				std::memcpy(config.encryptionKdfSalt, decoded.data(), copyLen);
			}

			//set encrypt mode
			int encryptRet = engine->enableEncryption(true, config);
			if (encryptRet != 0)
			{
				std::cerr << "FAILED: engine->enableEncryption returned " << getAgoraError(encryptRet) << std::endl;
			}

			std::cout << "Enabled encryption." << std::endl;
		}
	}

	// 4. Join and Record
	int joinRes = -1;
	if (agoraConfig.userId.empty())
	{
		joinRes = engine->joinChannel(agoraConfig.token.c_str(), agoraConfig.channel.c_str(), 0, options);
	}
	else
	{
		int localAccRes = engine->registerLocalUserAccount(agoraConfig.appId.c_str(), agoraConfig.userId.c_str());
		if (localAccRes != 0)
		{
			std::cerr << "FAILED: engine->registerLocalUserAccount returned " << getAgoraError(localAccRes) << std::endl;
		}

		joinRes = engine->joinChannelWithUserAccount(agoraConfig.token.c_str(), agoraConfig.channel.c_str(), agoraConfig.userId.c_str(), options);
	}

	if (joinRes != 0)
	{
		std::cerr << "FAILED: engine->joinChannel returned " << getAgoraError(joinRes) << std::endl;
		WaitInput();
		return -1;
	}

	std::cout << "Successfully joined channel." << std::endl;

	// 5. THE MESSAGE LOOP
	MSG msg = {};
	bool running = true;

	while (running)
	{
		// Handle Windows Messages (Rendering the video)
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (myEventHandler.UserLeft)
		{
			// Check if recorders are inactive
			auto now = std::chrono::steady_clock::now();
			auto lastActive = ctx->lastActivityTime.load();
			auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - lastActive).count();
			if (elapsedSeconds > 10)
			{
				running = false;
			}
		}

		Sleep(10); // Don't burn 100% CPU
	}

	// 5. Cleanup
	std::cout << "Cleaning up" << std::endl;
	audioObserver.Cleanup();
	videoObserver.Cleanup();
	rawFrameObserver.Cleanup();

	engine->leaveChannel();

	engine->release();

	if (!agoraConfig.viewer)
	{
		// merge the video
		if (0)
		{
			std::cout << "Merging video" << std::endl;
			_chdir(folderName.c_str());
			std::cout << "Changed dir to: " << folderName.c_str() << std::endl;

			// parse raw video
			{
				// std::string videoCmd = "mkvmerge -o video_only.mkv --timestamps 0:video_ts.txt remote_host_stream.h264";
				std::stringstream ss;
				ss << "mkvmerge -o video_only.mkv --timestamps 0:" << VIDEOTS_NAME << " " << VIDEO_NAME;
				std::string videoCmd = ss.str();
				executeCmd(videoCmd);
			}

			// parse raw audio
			{
				// std::string audioCmd = "ffmpeg -f s16le -ar 48000 -ac 2 -i remote_stream.pcm remote_stream.aac";

				std::stringstream ss;
				ss << "ffmpeg -f s16le -ar 48000 -ac 2 -i " << AUDIO_NAME << " audio.aac";
				std::string audioCmd = ss.str();
				executeCmd(audioCmd);
			}

			// std::string mergeCmd = "ffmpeg -y -i video_only.mkv -itsoffset -0.801 -i remote_stream.aac -c copy -map 0:v:0 -map 1:a:0 -shortest final_output.mp4";

			// merge audio and video
			{
				uint64_t offset = videoObserver.firstVideoTS - audioObserver.firstAudioTS;
				double offsetSeconds = offset / 1000.0;
				// std::string mergeCmd = std::snprintf("ffmpeg -y -i video_only.mkv -itsoffset -%.3f -i remote_stream.aac -c copy -map 0:v:0 -map 1:a:0 -shortest final_output.mp4", offsetSeconds);
				std::stringstream ss;
				ss << "ffmpeg -y -i video_only.mkv -ss " << offsetSeconds << " -i audio.aac -c copy -map 0:v:0 -map 1:a:0 -shortest output.mkv";
				// ss << "ffmpeg -y -i video_only.mkv -itsoffset " << offsetSeconds << " -i audio.aac -c copy -map 0:v:0 -map 1:a:0 output.mkv";

				std::string mergeCmd = ss.str();
				executeCmd(mergeCmd);
			}
		}

		_chdir(folderName.c_str());
		std::string mergeCmd = "agora_merge.py"; 
		std::cout << "Merging video with " << mergeCmd << std::endl;
		executeCmd(mergeCmd);
	}

	WaitInput();

	return 0;
}