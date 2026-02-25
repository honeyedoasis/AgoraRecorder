#pragma once
#include <AgoraBase.h>
#include <string>

using namespace agora::rtc;


namespace utils
{
    struct AgoraErrorInfo
    {
        int code;
        std::string id;
        std::string message;

        std::string toString() const
        {
            return "[Error " + std::to_string(code) + "] " + id + ": " + message;
        }
    };

    inline AgoraErrorInfo getAgoraErrorInfo(int errorCode)
    {
        std::string id;
        std::string msg;

        errorCode = std::abs(errorCode);

        switch (errorCode)
        {
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

    inline std::string getAgoraError(int errorCode)
    {
        return getAgoraErrorInfo(errorCode).toString();
    }

    inline const char* OfflineReasonToString(USER_OFFLINE_REASON_TYPE reason)
    {
        switch (reason)
        {
        case USER_OFFLINE_QUIT: return "User left the channel (Normal)";
        case USER_OFFLINE_DROPPED: return "Connection dropped (Timeout)";
        case USER_OFFLINE_BECOME_AUDIENCE: return "User changed role to Audience";
        default: return "Unknown";
        }
    }
}
