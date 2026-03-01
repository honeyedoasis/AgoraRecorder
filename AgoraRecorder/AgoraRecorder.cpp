#include <iostream>
#include <string>
#include <IAgoraRtcEngine.h>
#include <IAgoraMediaRecorder.h>
#include <AgoraRefPtr.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <windows.h>

#include "MyBase64.h"
#include "Utils.h"

using namespace agora::rtc;
using namespace agora::media;
using namespace utils;

void WaitInput()
{
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
        loadFromFile("config.ini");

        // 2. Overwrite with command line arguments (CLI takes priority)
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

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
};

class MyRecorderObserver : public IMediaRecorderObserver
{
public:
    // Helper to turn State enum into string
    const char* StateToString(RecorderState state)
    {
        switch (state)
        {
        case RECORDER_STATE_ERROR: return "ERROR";
        case RECORDER_STATE_START: return "STARTING/STARTED";
        case RECORDER_STATE_STOP: return "STOPPED";
        default: return "UNKNOWN";
        }
    }

    // Helper to turn Reason enum into string
    const char* ReasonToString(RecorderReasonCode reason)
    {
        switch (reason)
        {
        case RECORDER_REASON_NONE: return "None (Success)";
        case RECORDER_REASON_WRITE_FAILED: return "File Write Failed";
        case RECORDER_REASON_NO_STREAM: return "No Audio/Video Stream";
        case RECORDER_REASON_OVER_MAX_DURATION: return "Max Duration Reached";
        case RECORDER_REASON_CONFIG_CHANGED: return "Configuration Changed";
        default: return "Internal Error";
        }
    }

    void onRecorderStateChanged(const char* channelId, uid_t uid, RecorderState state, RecorderReasonCode reason) override
    {
        std::cout << "\n[RECORDER EVENT]" << std::endl;
        std::cout << "  Channel: " << (channelId ? channelId : "N/A") << std::endl;
        std::cout << "  User UID: " << uid << std::endl;
        std::cout << "  New State: " << StateToString(state) << " (" << (int)state << ")" << std::endl;
        std::cout << "  Reason: " << ReasonToString(reason) << " (" << (int)reason << ")" << std::endl;
    }

    void onRecorderInfoUpdated(const char* channelId, uid_t uid, const RecorderInfo& info) override
    {
        // This triggers periodically (roughly every second) during recording
        std::cout << "[RECORDER INFO] Filename: " << info.fileName
            << " | Duration: " << info.durationMs << "ms"
            << " | Size: " << info.fileSize << " bytes" << std::endl;
    }
};

class MyEventHandler : public IRtcEngineEventHandler
{
public:
    IRtcEngine* engine;
    AgoraConfig agoraConfig;
    HWND videoHwnd;
    IMediaRecorder* recorder;
    MyRecorderObserver recorderObserver;

    ~MyEventHandler() override
    {
        stopRecording();
    }

    void stopRecording()
    {
        if (recorder)
        {
            recorder->stopRecording();

            if (engine)
            {
                engine->destroyMediaRecorder(recorder);
            }

            recorder = nullptr;
        }
    }

    void onUserJoined(uid_t uid, int elapsed) override
    {
        std::cout << "[EVENT] User joined: " << uid << std::endl;

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

        // recording part
        {
            RecorderStreamInfo streamInfo;
            streamInfo.channelId = agoraConfig.channel.c_str();
            streamInfo.uid = uid;
            recorder = engine->createMediaRecorder(streamInfo).get();
            MediaRecorderConfiguration config;

            auto now = std::chrono::system_clock::now();
            std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

            std::tm utc_tm = *std::gmtime(&now_time_t);

            std::ostringstream oss;
            oss << std::put_time(&utc_tm, "%Y%m%d_%H%M%S_");

            std::string outPath = oss.str() + agoraConfig.channel + ".mp4";
            config.storagePath = outPath.c_str();
            recorder->setMediaRecorderObserver(&recorderObserver);
            int recordRet = recorder->startRecording(config);
            if (recordRet != 0)
            {
                std::cerr << "  StartRecording failed: " << ret << std::endl;
            }
            else
            {
                std::cout << "  Recording success" << std::endl;
            }
        }
    }

    void onUserOffline(uid_t uid, USER_OFFLINE_REASON_TYPE reason) override
    {
        std::cout << "[EVENT] User Offline" << std::endl;
        std::cout << "  UID: " << uid << std::endl;
        std::cout << "  Reason: " << OfflineReasonToString(reason) << " (" << reason << ")" << std::endl;

        // // 1. Tell Agora to stop rendering to this window
        // VideoCanvas canvas;
        // canvas.view = nullptr; // Set view to null to unbind
        // canvas.uid = uid;
        // engine->setupRemoteVideo(canvas);

        // 2. Force the window to redraw itself (which will trigger WM_PAINT)
        // This clears the "frozen" last frame.
        InvalidateRect(videoHwnd, nullptr, true);
        UpdateWindow(videoHwnd);

        stopRecording();

        // std::cout << "Host left, exiting application..." << std::endl;
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

        stopRecording();
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
    switch (uMsg)
    {
    case WM_DESTROY:
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
    HWND videoHwnd = CreateVideoWindow(800, 600);

    myEventHandler.engine = engine;
    myEventHandler.videoHwnd = videoHwnd;
    myEventHandler.agoraConfig = agoraConfig;

    RtcEngineContext context;
    context.appId = agoraConfig.appId.c_str();
    context.eventHandler = &myEventHandler;
    context.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;

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
    engine->startPreview();

    ChannelMediaOptions options;
    options.enableBuiltInMediaEncryption = true;
    options.channelProfile = agora::CHANNEL_PROFILE_LIVE_BROADCASTING;
    options.clientRoleType = CLIENT_ROLE_AUDIENCE;
    options.autoSubscribeAudio = true;
    options.autoSubscribeVideo = true;
    if (!agoraConfig.token.empty())
    {
        options.token = agoraConfig.token.c_str();
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

        // Check if the user pressed a key in the Console
        // Using a simpler non-blocking check
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            std::cout << "Escape key detected, exiting..." << std::endl;
            running = false;
        }

        Sleep(10); // Don't burn 100% CPU
    }

    // 5. Cleanup
    std::cout << "Cleaning up" << std::endl;

    engine->leaveChannel();

    myEventHandler.stopRecording();

    engine->release();

    std::cout << "Shut down. Press Enter to exit..." << std::endl;

    WaitInput();

    return 0;
}
