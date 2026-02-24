#include <iostream>
#include <string>
#include <IAgoraRtcEngine.h>
#include <IAgoraMediaRecorder.h>
#include <AgoraRefPtr.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <windows.h>

#include "MyBase64.h"

using namespace agora::rtc;
using namespace agora::media;

struct AgoraErrorInfo {
    int code;
    std::string id;
    std::string message;

    std::string toString() const {
        return "[Error " + std::to_string(code) + "] " + id + ": " + message;
    }
};

AgoraErrorInfo getAgoraErrorInfo(int errorCode) {
    std::string id;
    std::string msg;

    switch (errorCode) {
        case 0:
            id = "ERR_OK";
            msg = "No error.";
            break;
        case 1:
            id = "ERR_FAILED";
            msg = "General error with no classified reason. Try calling the method again.";
            break;
        case 2:
            id = "ERR_INVALID_ARGUMENT";
            msg = "An invalid parameter is used. Reset the parameter.";
            break;
        case 3:
            id = "ERR_NOT_READY";
            msg = "The SDK is not ready. Check initialization, channel status, or audio module.";
            break;
        case 4:
            id = "ERR_NOT_SUPPORTED";
            msg = "The IRtcEngine does not support the request. Check encryption settings.";
            break;
        case 5:
            id = "ERR_REFUSED";
            msg = "The request is rejected. Check initialization or channel name validity.";
            break;
        case 6:
            id = "ERR_BUFFER_TOO_SMALL";
            msg = "The buffer size is insufficient to store the returned data.";
            break;
        case 7:
            id = "ERR_NOT_INITIALIZED";
            msg = "A method is called before the initialization of IRtcEngine.";
            break;
        case 8:
            id = "ERR_INVALID_STATE";
            msg = "Invalid state.";
            break;
        case 9:
            id = "ERR_NO_PERMISSION";
            msg = "Permission to access is not granted. Check audio/video device permissions.";
            break;
        case 10:
            id = "ERR_TIMEDOUT";
            msg = "A timeout occurs (SDK took more than 10 seconds to return result).";
            break;
        case 17:
            id = "ERR_JOIN_CHANNEL_REJECTED";
            msg = "The request to join the channel is rejected (User already in channel or in echo test).";
            break;
        case 18:
            id = "ERR_LEAVE_CHANNEL_REJECTED";
            msg = "Fails to leave the channel.";
            break;
        case 19:
            id = "ERR_ALREADY_IN_USE";
            msg = "Resources are already in use.";
            break;
        case 20:
            id = "ERR_ABORTED";
            msg = "The request is abandoned by the SDK, possibly due to high frequency.";
            break;
        case 21:
            id = "ERR_INIT_NET_ENGINE";
            msg = "IRtcEngine fails to initialize due to Windows firewall settings.";
            break;
        case 22:
            id = "ERR_RESOURCE_LIMITED";
            msg = "The SDK fails to allocate resources due to system limitations.";
            break;
        case 101:
            id = "ERR_INVALID_APP_ID";
            msg = "The specified App ID is invalid.";
            break;
        case 102:
            id = "ERR_INVALID_CHANNEL_NAME";
            msg = "The specified channel name is invalid.";
            break;
        case 103:
            id = "ERR_NO_SERVER_RESOURCES";
            msg = "Fails to get server resources in the specified region.";
            break;
        case 109:
            id = "ERR_TOKEN_EXPIRED";
            msg = "Deprecated: The current token has expired.";
            break;
        case 110:
            id = "ERR_INVALID_TOKEN";
            msg = "Deprecated: Invalid token. Check App Certificate and UID.";
            break;
        case 111:
            id = "ERR_CONNECTION_INTERRUPTED";
            msg = "The network connection is interrupted.";
            break;
        case 112:
            id = "ERR_CONNECTION_LOST";
            msg = "The network connection is lost.";
            break;
        case 113:
            id = "ERR_NOT_IN_CHANNEL";
            msg = "The user is not in the channel when calling the method.";
            break;
        case 114:
            id = "ERR_SIZE_TOO_LARGE";
            msg = "The data size exceeds 1 KB.";
            break;
        case 115:
            id = "ERR_BITRATE_LIMIT";
            msg = "The data bitrate exceeds 6 KB/s.";
            break;
        case 116:
            id = "ERR_TOO_MANY_DATA_STREAMS";
            msg = "More than five data streams are created.";
            break;
        case 117:
            id = "ERR_STREAM_MESSAGE_TIMEOUT";
            msg = "The data stream transmission times out.";
            break;
        case 119:
            id = "ERR_SET_CLIENT_ROLE_NOT_AUTHORIZED";
            msg = "Switching roles fails, try rejoining the channel.";
            break;
        case 120:
            id = "ERR_DECRYPTION_FAILED";
            msg = "Media streams decryption fails. Check password.";
            break;
        case 121:
            id = "ERR_INVALID_USER_ID";
            msg = "The user ID is invalid.";
            break;
        case 122:
            id = "ERR_DATASTREAM_DECRYPTION_FAILED";
            msg = "Data streams decryption fails. Check password.";
            break;
        case 123:
            id = "ERR_CLIENT_IS_BANNED_BY_SERVER";
            msg = "The user is banned from the server.";
            break;
        case 130:
            id = "ERR_ENCRYPTED_STREAM_NOT_ALLOWED_PUBLISH";
            msg = "The SDK does not support pushing encrypted streams to CDN.";
            break;
        case 134:
            id = "ERR_INVALID_USER_ACCOUNT";
            msg = "The user account is invalid.";
            break;
        case 1001:
            id = "ERR_LOAD_MEDIA_ENGINE";
            msg = "The SDK fails to load the media engine.";
            break;
        case 1005:
            id = "ERR_ADM_GENERAL_ERROR";
            msg = "Audio Device Module: General error. Check if device is in use.";
            break;
        case 1008:
            id = "ERR_ADM_INIT_PLAYOUT";
            msg = "Audio Device Module: Error initializing playback device.";
            break;
        case 1009:
            id = "ERR_ADM_START_PLAYOUT";
            msg = "Audio Device Module: Error starting playback device.";
            break;
        case 1010:
            id = "ERR_ADM_STOP_PLAYOUT";
            msg = "Audio Device Module: Error stopping playback device.";
            break;
        case 1011:
            id = "ERR_ADM_INIT_RECORDING";
            msg = "Audio Device Module: Error initializing recording device.";
            break;
        case 1012:
            id = "ERR_ADM_START_RECORDING";
            msg = "Audio Device Module: Error starting recording device.";
            break;
        case 1013:
            id = "ERR_ADM_STOP_RECORDING";
            msg = "Audio Device Module: Error stopping recording device.";
            break;
        case 1501:
            id = "ERR_VDM_CAMERA_NOT_AUTHORIZED";
            msg = "Video Device Module: Permission to access the camera is not granted.";
            break;
        default:
            id = "ERR_UNKNOWN_ERROR";
            msg = "An undefined error occurred.";
            break;
    }

    return {errorCode, id, msg};
}

const char* OfflineReasonToString(USER_OFFLINE_REASON_TYPE reason)
{
    switch (reason)
    {
    case USER_OFFLINE_QUIT: return "User left the channel (Normal)";
    case USER_OFFLINE_DROPPED: return "Connection dropped (Timeout)";
    case USER_OFFLINE_BECOME_AUDIENCE: return "User changed role to Audience";
    default: return "Unknown";
    }
}

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

    AgoraConfig(int argc, char* argv[])
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            size_t pos = arg.find('=');
            if (pos == std::string::npos)
            {
                continue;
            }

            std::string key = arg.substr(0, pos);
            std::string val = arg.substr(pos + 1);

            if (key == "-app_id") appId = val;
            else if (key == "-channel_name") channel = val;
            else if (key == "-token") token = val;
            // else if (key == "-user_id") uid = static_cast<unsigned int>(std::stoul(val)); // Note: Agora UIDs must be numeric
            else if (key == "-user_id") userId = val;
            else if (key == "-encryption_key") encryptionKey = val;
            else if (key == "-encryption_salt") encryptionSalt = val;
        }
    }

    void logConfig() {
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
            engine->destroyMediaRecorder(recorder);
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
            std::cerr << "  SetupRemoteVideo failed: " << ret << std::endl;
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
    std::cout << "Start AgoraCLI" << std::endl;
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

    engine->initialize(context);
    int ret = engine->initialize(context);
    if (ret != 0)
    {
        std::cerr << "FAILED: engine->initialize returned " << ret << std::endl;
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

            // config.encryptionKey = "b8619de8b47f6d450ee6c85377ea635bcf8225ba1a14b56c93cb244895b938e0";
            // std::string kdfSaltBase64 = "mNPCRk6cRLOkC0fJeoTlapzcM0hcWygfL8u1cqkpbRs=";

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
                std::cerr << "FAILED: engine->enableEncryption returned " << encryptRet << std::endl;
            }

            std::cout << "Enabled encryption." << std::endl;
        }
    }

    // 4. Join and Record
    // agoraConfig.uid
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
        std::cerr << "FAILED: engine->joinChannel returned " << joinRes << std::endl;
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
    // recorder->stopRecording();
    engine->leaveChannel();
    engine->release();

    std::cout << "Shut down. Press Enter to exit..." << std::endl;
    
    WaitInput();

    return 0;
}
