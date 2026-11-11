#include "src/camera/uvc/uvc_camera.h"
#include "src/core/clock.h"
#include "src/core/logger.h"

#include <windows.h>
#include <dshow.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <strmif.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>

#ifndef MF_LOW_LATENCY
// {9C27891A-ED7A-40E1-88E8-B22727A024EE}
static const GUID SABIT_MF_LOW_LATENCY = {0x9c27891a, 0xed7a, 0x40e1, {0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee}};
#define MF_LOW_LATENCY SABIT_MF_LOW_LATENCY
#endif

namespace sabit::camera {

namespace {

struct MFScope {
    MFScope() {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        MFStartup(MF_VERSION);
    }
    ~MFScope() {
        MFShutdown();
        CoUninitialize();
    }
};

} // namespace

UvcCamera::UvcCamera() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION);
}

UvcCamera::~UvcCamera() {
    close();
    MFShutdown();
    CoUninitialize();
}

void UvcCamera::release_mf_resources() {
    if (mf_reader_) {
        reinterpret_cast<IMFSourceReader*>(mf_reader_)->Release();
        mf_reader_ = nullptr;
    }
    if (mf_source_) {
        reinterpret_cast<IMFMediaSource*>(mf_source_)->Shutdown();
        reinterpret_cast<IMFMediaSource*>(mf_source_)->Release();
        mf_source_ = nullptr;
    }
}

namespace {

std::string normalize_device_name(std::string s) {
    // Normalize full-width ampersand (\xEF\xBC\x86) to standard ASCII '&'
    const std::string fw_amp = "\xEF\xBC\x86";
    size_t pos = 0;
    while ((pos = s.find(fw_amp, pos)) != std::string::npos) {
        s.replace(pos, fw_amp.length(), "&");
        pos += 1;
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string lower_ascii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string wide_to_utf8_clean(const WCHAR* wstr) {
    if (!wstr || !*wstr) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), needed, NULL, NULL);
    return normalize_device_name(out);
}

void enum_registry_dshow_filters(HKEY root_key, REGSAM sam_view, std::vector<CameraId>& list, std::set<std::string>& seen_lower) {
    const WCHAR* subkey = L"SOFTWARE\\Classes\\CLSID\\{860BB310-5D01-11d0-BD3B-00A0C911CE86}\\Instance";
    HKEY hInst = NULL;
    if (RegOpenKeyExW(root_key, subkey, 0, KEY_READ | sam_view, &hInst) != ERROR_SUCCESS) {
        return;
    }
    WCHAR child_name[256];
    DWORD idx = 0;
    while (true) {
        DWORD child_len = 256;
        if (RegEnumKeyExW(hInst, idx++, child_name, &child_len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break;
        }
        HKEY hItem = NULL;
        if (RegOpenKeyExW(hInst, child_name, 0, KEY_READ | sam_view, &hItem) == ERROR_SUCCESS) {
            WCHAR fname[256] = {};
            DWORD cb = sizeof(fname);
            DWORD type = 0;
            if (RegQueryValueExW(hItem, L"FriendlyName", NULL, &type, reinterpret_cast<LPBYTE>(fname), &cb) == ERROR_SUCCESS &&
                (type == REG_SZ || type == REG_EXPAND_SZ)) {
                std::string clean = wide_to_utf8_clean(fname);
                std::string key = lower_ascii(clean);
                if (!clean.empty() && seen_lower.find(key) == seen_lower.end()) {
                    seen_lower.insert(key);
                    list.push_back(CameraId{
                        .driver = "uvc",
                        .id = "dshow:" + clean,
                        .name = clean
                    });
                }
            }
            RegCloseKey(hItem);
        }
    }
    RegCloseKey(hInst);
}

void enum_registry_pnp_video_classes(std::vector<CameraId>& list, std::set<std::string>& seen_lower) {
    // KSCATEGORY_VIDEO and KSCATEGORY_VIDEO_CAMERA store all connected & historical video capture hardware
    const WCHAR* class_paths[] = {
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\{6994AD05-93EF-11D0-A3CC-00A0C9223196}",
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\{e5323777-f976-4f5b-9b55-b94699c46e44}"
    };
    for (const WCHAR* base_path : class_paths) {
        HKEY hClass = NULL;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, base_path, 0, KEY_READ | KEY_WOW64_64KEY, &hClass) != ERROR_SUCCESS) {
            continue;
        }
        WCHAR dev_key_name[512];
        DWORD idx = 0;
        while (true) {
            DWORD dev_len = 512;
            if (RegEnumKeyExW(hClass, idx++, dev_key_name, &dev_len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            std::wstring global_param = std::wstring(dev_key_name) + L"\\#GLOBAL\\Device Parameters";
            HKEY hParam = NULL;
            if (RegOpenKeyExW(hClass, global_param.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &hParam) == ERROR_SUCCESS) {
                WCHAR fname[256] = {};
                DWORD cb = sizeof(fname);
                DWORD type = 0;
                if (RegQueryValueExW(hParam, L"FriendlyName", NULL, &type, reinterpret_cast<LPBYTE>(fname), &cb) == ERROR_SUCCESS &&
                    (type == REG_SZ || type == REG_EXPAND_SZ)) {
                    std::string clean = wide_to_utf8_clean(fname);
                    std::string key = lower_ascii(clean);
                    if (!clean.empty() && seen_lower.find(key) == seen_lower.end()) {
                        seen_lower.insert(key);
                        list.push_back(CameraId{
                            .driver = "uvc",
                            .id = "pnp:" + clean,
                            .name = clean
                        });
                    }
                }
                RegCloseKey(hParam);
            }
        }
        RegCloseKey(hClass);
    }
}

} // namespace

std::vector<CameraId> UvcCamera::enumerate_cameras() {
    std::vector<CameraId> list;
    std::set<std::string> seen_lower;
    MFScope scope;

    // 1. Active Media Foundation Video Capture Sources
    IMFAttributes* pAttributes = nullptr;
    if (SUCCEEDED(MFCreateAttributes(&pAttributes, 1))) {
        if (SUCCEEDED(pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) {
            IMFActivate** ppDevices = nullptr;
            UINT32 count = 0;
            if (SUCCEEDED(MFEnumDeviceSources(pAttributes, &ppDevices, &count)) && count > 0) {
                for (UINT32 i = 0; i < count; ++i) {
                    WCHAR* name = nullptr;
                    UINT32 len = 0;
                    std::string friendly_name = "UVC Camera " + std::to_string(i);
                    if (SUCCEEDED(ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len)) && name) {
                        std::string clean = wide_to_utf8_clean(name);
                        if (!clean.empty()) friendly_name = clean;
                        CoTaskMemFree(name);
                    }
                    std::string key = lower_ascii(friendly_name);
                    if (seen_lower.find(key) == seen_lower.end()) {
                        seen_lower.insert(key);
                        list.push_back(CameraId{
                            .driver = "uvc",
                            .id = std::to_string(i),
                            .name = friendly_name
                        });
                    }
                    ppDevices[i]->Release();
                }
                CoTaskMemFree(ppDevices);
            }
        }
        pAttributes->Release();
    }

    // 2. DirectShow System Device Enumerator (CLSID_VideoInputDeviceCategory) — matches SharpCap DirectShow enumeration
    ICreateDevEnum* pDevEnum = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, reinterpret_cast<void**>(&pDevEnum))) && pDevEnum) {
        IEnumMoniker* pEnum = nullptr;
        if (pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0) == S_OK && pEnum) {
            IMoniker* pMoniker = nullptr;
            ULONG fetched = 0;
            while (pEnum->Next(1, &pMoniker, &fetched) == S_OK && pMoniker) {
                IPropertyBag* pPropBag = nullptr;
                if (SUCCEEDED(pMoniker->BindToStorage(0, 0, IID_IPropertyBag, reinterpret_cast<void**>(&pPropBag))) && pPropBag) {
                    VARIANT varName;
                    VariantInit(&varName);
                    if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, 0)) && varName.vt == VT_BSTR && varName.bstrVal) {
                        std::string clean = wide_to_utf8_clean(varName.bstrVal);
                        std::string key = lower_ascii(clean);
                        if (!clean.empty() && seen_lower.find(key) == seen_lower.end()) {
                            seen_lower.insert(key);
                            list.push_back(CameraId{
                                .driver = "uvc",
                                .id = "dshow:" + clean,
                                .name = clean
                            });
                        }
                    }
                    VariantClear(&varName);
                    pPropBag->Release();
                }
                pMoniker->Release();
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }

    // 3. Registered DirectShow Video Input Filters (64-bit, 32-bit WOW6432Node, and HKCU)
    enum_registry_dshow_filters(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, list, seen_lower);
    enum_registry_dshow_filters(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, list, seen_lower);
    enum_registry_dshow_filters(HKEY_CURRENT_USER, 0, list, seen_lower);

    // 4. Historical & PnP Video Capture DeviceClasses (detects cameras currently or previously connected to PC)
    enum_registry_pnp_video_classes(list, seen_lower);

    return list;
}

core::Result<CameraCaps> UvcCamera::open(const CameraId& id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        close();
    }

    int requested_idx = -1;
    try {
        size_t pos = 0;
        int parsed = std::stoi(id.id, &pos);
        if (pos == id.id.size()) {
            requested_idx = parsed;
        }
    } catch (...) {
        requested_idx = -1;
    }
    device_name_ = id.name.empty() ? ("UVC Camera " + std::to_string(std::max(0, requested_idx))) : normalize_device_name(id.name);

    IMFAttributes* pAttributes = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) {
        return core::Result<CameraCaps>::err(core::Error(core::ErrorCode::HardwareError, "Failed to create MF attributes"));
    }

    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        pAttributes->Release();
        return core::Result<CameraCaps>::err(core::Error(core::ErrorCode::HardwareError, "Failed to set MF attribute GUID"));
    }

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    pAttributes->Release();

    if (FAILED(hr) || count == 0) {
        return core::Result<CameraCaps>::err(core::Error(
            core::ErrorCode::NotFound,
            "Perangkat kamera '" + device_name_ + "' sedang tidak aktif atau belum terhubung ke port USB."
        ));
    }

    // Match by FriendlyName first (most reliable across re-plugging and mixed DirectShow/MF lists)
    int matched_idx = -1;
    std::string target_key = lower_ascii(device_name_);
    for (UINT32 i = 0; i < count; ++i) {
        WCHAR* name = nullptr;
        UINT32 len = 0;
        if (SUCCEEDED(ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &len)) && name) {
            std::string clean = wide_to_utf8_clean(name);
            CoTaskMemFree(name);
            if (!target_key.empty() && lower_ascii(clean) == target_key) {
                matched_idx = static_cast<int>(i);
                device_name_ = clean;
                break;
            }
        }
    }

    if (matched_idx < 0 && requested_idx >= 0 && static_cast<UINT32>(requested_idx) < count) {
        matched_idx = requested_idx;
    }

    if (matched_idx < 0) {
        for (UINT32 i = 0; i < count; ++i) ppDevices[i]->Release();
        CoTaskMemFree(ppDevices);
        return core::Result<CameraCaps>::err(core::Error(
            core::ErrorCode::NotFound,
            "Kamera '" + device_name_ + "' terdaftar di sistem tetapi saat ini tidak aktif / belum dicolokkan."
        ));
    }

    device_index_ = matched_idx;
    IMFMediaSource* pSource = nullptr;
    hr = ppDevices[device_index_]->ActivateObject(IID_PPV_ARGS(&pSource));

    for (UINT32 i = 0; i < count; ++i) ppDevices[i]->Release();
    CoTaskMemFree(ppDevices);

    if (FAILED(hr) || !pSource) {
        return core::Result<CameraCaps>::err(core::Error(
            core::ErrorCode::HardwareError,
            "Gagal mengaktifkan '" + device_name_ + "' (Perangkat sedang dipakai aplikasi lain atau tidak merespons)."
        ));
    }

    mf_source_ = pSource;

    IMFAttributes* pReaderAttrs = nullptr;
    if (SUCCEEDED(MFCreateAttributes(&pReaderAttrs, 4))) {
        pReaderAttrs->SetUINT32(MF_LOW_LATENCY, TRUE); // Zero-latency buffering (equivalent to CAP_PROP_BUFFERSIZE = 1)
        pReaderAttrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        pReaderAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromMediaSource(pSource, pReaderAttrs, &pReader);
    if (pReaderAttrs) pReaderAttrs->Release();
    if (FAILED(hr) || !pReader) {
        release_mf_resources();
        return core::Result<CameraCaps>::err(core::Error(core::ErrorCode::HardwareError, "Failed to create MF Source Reader"));
    }
    mf_reader_ = pReader;

    // Scan native formats: prioritize 1080p 60fps/30fps YUY2 or MJPG, then highest resolution
    DWORD native_idx = 0;
    IMFMediaType* pNative = nullptr;
    UINT32 best_w = 0, best_h = 0;
    int best_score = -1;

    while (SUCCEEDED(pReader->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), native_idx++, &pNative))) {
        UINT32 nw = 0, nh = 0;
        MFGetAttributeSize(pNative, MF_MT_FRAME_SIZE, &nw, &nh);
        UINT32 fr_num = 0, fr_den = 1;
        MFGetAttributeRatio(pNative, MF_MT_FRAME_RATE, &fr_num, &fr_den);
        double fps = (fr_den > 0) ? (static_cast<double>(fr_num) / fr_den) : 30.0;
        GUID n_sub = GUID_NULL;
        pNative->GetGUID(MF_MT_SUBTYPE, &n_sub);

        int score = 0;
        if (IsEqualGUID(n_sub, MFVideoFormat_YUY2)) score += 1000;
        else if (IsEqualGUID(n_sub, MFVideoFormat_MJPG)) score += 850;
        else if (IsEqualGUID(n_sub, MFVideoFormat_NV12)) score += 600;
        else if (IsEqualGUID(n_sub, MFVideoFormat_RGB32) || IsEqualGUID(n_sub, MFVideoFormat_RGB24)) score += 400;

        if (nw == 1920 && nh == 1080) score += 600;
        else if (nw == 1280 && nh == 720) score += 300;
        else score += std::clamp<int>(static_cast<int>(nw * nh / 10000), 0, 250);

        if (fps >= 59.0) score += 200;
        else if (fps >= 29.0) score += 100;

        if (score > best_score) {
            best_score = score;
            best_w = nw;
            best_h = nh;
        }
        pNative->Release();
    }

    // Explicitly configure YUY2, RGB32, RGB24, or NV12 with the optimal resolution
    IMFMediaType* pType = nullptr;
    if (SUCCEEDED(MFCreateMediaType(&pType))) {
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        if (best_w > 0 && best_h > 0) {
            MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, best_w, best_h);
        }
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
        if (FAILED(pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), NULL, pType))) {
            pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
            if (FAILED(pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), NULL, pType))) {
                pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
                if (FAILED(pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), NULL, pType))) {
                    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
                    pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), NULL, pType);
                }
            }
        }
        pType->Release();
    }

    // Query current resolution, subtype, and stride
    UINT32 width = 1280;
    UINT32 height = 720;
    video_format_ = 0;
    default_stride_ = 0;
    IMFMediaType* pCurType = nullptr;
    if (SUCCEEDED(pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &pCurType))) {
        MFGetAttributeSize(pCurType, MF_MT_FRAME_SIZE, &width, &height);
        GUID subtype = GUID_NULL;
        if (SUCCEEDED(pCurType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            if (IsEqualGUID(subtype, MFVideoFormat_RGB32) || IsEqualGUID(subtype, MFVideoFormat_ARGB32)) {
                video_format_ = 1;
            } else if (IsEqualGUID(subtype, MFVideoFormat_RGB24)) {
                video_format_ = 2;
            } else if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
                video_format_ = 3;
            } else if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
                video_format_ = 4;
            }
        }
        UINT32 stride_u32 = 0;
        if (SUCCEEDED(pCurType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_u32))) {
            default_stride_ = static_cast<INT32>(stride_u32);
        }
        pCurType->Release();
    }

    // Probe hardware IAMCameraControl / IAMVideoProcAmp capabilities
    hw_exp_supported_ = false;
    hw_gain_supported_ = false;
    {
        IAMCameraControl* pCamControl = nullptr;
        if (SUCCEEDED(pSource->QueryInterface(IID_PPV_ARGS(&pCamControl))) && pCamControl) {
            long min_v = 0, max_v = 0, step_v = 0, def_v = 0, flags_v = 0;
            if (SUCCEEDED(pCamControl->GetRange(CameraControl_Exposure, &min_v, &max_v, &step_v, &def_v, &flags_v))) {
                hw_exp_supported_ = true;
            }
            pCamControl->Release();
        }
        IAMVideoProcAmp* pProcAmp = nullptr;
        if (SUCCEEDED(pSource->QueryInterface(IID_PPV_ARGS(&pProcAmp))) && pProcAmp) {
            long min_v = 0, max_v = 0, step_v = 0, def_v = 0, flags_v = 0;
            if (SUCCEEDED(pProcAmp->GetRange(VideoProcAmp_Gain, &min_v, &max_v, &step_v, &def_v, &flags_v))) {
                hw_gain_supported_ = true;
            }
            pProcAmp->Release();
        }
    }

    caps_.model = device_name_;
    caps_.serial = "UVC-" + std::to_string(device_index_);
    caps_.driver = "uvc";
    caps_.sdk_version = "MediaFoundation-1.0";
    caps_.max_width = static_cast<int>(width);
    caps_.max_height = static_cast<int>(height);
    caps_.min_exposure_us = 1000;
    caps_.max_exposure_us = 1000000;
    caps_.min_gain = 0.0;
    caps_.max_gain = 300.0;
    caps_.supported_bins = {1};
    caps_.supported_bit_depths = {8};
    caps_.has_cooler = false;
    caps_.is_color = true;
    caps_.bayer = core::BayerPattern::None;

    caps_.capabilities.can_control_exposure = true;
    caps_.capabilities.can_control_gain = true;
    caps_.capabilities.can_control_wb = true;
    caps_.capabilities.can_control_roi = false;
    caps_.capabilities.can_control_pixel_format = false;
    caps_.capabilities.has_hardware_exposure = hw_exp_supported_;
    caps_.capabilities.has_hardware_gain = hw_gain_supported_;
    caps_.capabilities.has_software_fallback = true;
    caps_.capabilities.can_auto_exposure = hw_exp_supported_;
    caps_.capabilities.can_auto_gain = hw_gain_supported_;
    caps_.capabilities.can_crop_dslr_aspect = true;

    active_settings_.exposure_us = 33333; // ~30 fps nominal
    active_settings_.auto_exposure = false;
    active_settings_.gain = 0.0;
    active_settings_.auto_gain = false;
    active_settings_.wb_red = 50;
    active_settings_.wb_blue = 50;
    active_settings_.binning = 1;
    active_settings_.bit_depth = 8;
    active_settings_.roi = Roi{0, 0, caps_.max_width, caps_.max_height};

    is_open_ = true;
    is_disconnected_.store(false);

    return core::Result<CameraCaps>::ok(caps_);
}

void UvcCamera::close() {
    stop();
    std::lock_guard<std::mutex> lock(state_mutex_);
    release_mf_resources();
    is_open_ = false;
}

CameraCaps UvcCamera::caps() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return caps_;
}

CameraStatus UvcCamera::status() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    CameraStatus st{};
    st.is_connected = is_open_ && !is_disconnected_.load();
    st.is_streaming = is_streaming_.load();
    st.actual_fps = actual_fps_;
    st.frames_captured = frames_captured_;
    st.frames_dropped_hw = frames_dropped_hw_;
    return st;
}

core::Result<CameraSettings> UvcCamera::execute_apply_hardware(const CameraSettings& req) {
    if (!is_open_ || !mf_source_) {
        return core::Result<CameraSettings>::err(core::Error(core::ErrorCode::InvalidState, "Camera not open"));
    }

    IMFMediaSource* pSource = reinterpret_cast<IMFMediaSource*>(mf_source_);

    if (req.wb_red.has_value()) {
        active_settings_.wb_red = std::clamp(*req.wb_red, 0, 100);
    }
    if (req.wb_blue.has_value()) {
        active_settings_.wb_blue = std::clamp(*req.wb_blue, 0, 100);
    }
    if (req.auto_exposure.has_value()) {
        active_settings_.auto_exposure = *req.auto_exposure;
    }
    if (req.exposure_us.has_value()) {
        active_settings_.exposure_us = std::clamp<int64_t>(*req.exposure_us, caps_.min_exposure_us, caps_.max_exposure_us);
    }
    if (req.auto_gain.has_value()) {
        active_settings_.auto_gain = *req.auto_gain;
    }
    if (req.gain.has_value()) {
        active_settings_.gain = std::clamp(*req.gain, caps_.min_gain, caps_.max_gain);
    }
    if (req.crop_dslr_pillarbox.has_value()) {
        crop_dslr_pillarbox_ = *req.crop_dslr_pillarbox;
        active_settings_.crop_dslr_pillarbox = crop_dslr_pillarbox_;
    }

    // Only apply hardware calls if hardware exposure is genuinely supported (prevents driver freezes on webcams/capture cards)
    if (hw_exp_supported_) {
        IAMCameraControl* pCamControl = nullptr;
        if (SUCCEEDED(pSource->QueryInterface(IID_PPV_ARGS(&pCamControl))) && pCamControl) {
            bool auto_exp = active_settings_.auto_exposure.value_or(true);
            if (auto_exp) {
                pCamControl->Set(CameraControl_Exposure, -5, CameraControl_Flags_Auto);
            } else {
                int64_t exp_us = active_settings_.exposure_us.value_or(33333);
                double exp_sec = std::max(0.0001, static_cast<double>(exp_us) / 1e6);
                long exp_val = std::clamp<long>(static_cast<long>(std::lround(std::log2(exp_sec))), -13L, 0L);
                pCamControl->Set(CameraControl_Exposure, exp_val, CameraControl_Flags_Manual);
            }
            pCamControl->Release();
        }
    }

    // Only apply hardware calls if hardware gain is genuinely supported
    if (hw_gain_supported_) {
        IAMVideoProcAmp* pProcAmp = nullptr;
        if (SUCCEEDED(pSource->QueryInterface(IID_PPV_ARGS(&pProcAmp))) && pProcAmp) {
            bool auto_g = active_settings_.auto_gain.value_or(false);
            if (auto_g) {
                pProcAmp->Set(VideoProcAmp_Gain, 0, VideoProcAmp_Flags_Auto);
            } else {
                long gain_val = static_cast<long>(active_settings_.gain.value_or(0.0));
                pProcAmp->Set(VideoProcAmp_Gain, gain_val, VideoProcAmp_Flags_Manual);
            }
            pProcAmp->Release();
        }
    }

    return core::Result<CameraSettings>::ok(active_settings_);
}

core::Result<CameraSettings> UvcCamera::apply(const CameraSettings& settings) {
    if (!is_open_) {
        return core::Result<CameraSettings>::err(core::Error(core::ErrorCode::InvalidState, "UVC Camera is not connected"));
    }

    if (is_streaming_.load()) {
        std::lock_guard<std::mutex> lock(cmd_mutex_);
        if (settings.wb_red.has_value()) {
            active_settings_.wb_red = std::clamp(*settings.wb_red, 0, 100);
        }
        if (settings.wb_blue.has_value()) {
            active_settings_.wb_blue = std::clamp(*settings.wb_blue, 0, 100);
        }
        if (settings.auto_exposure.has_value()) {
            active_settings_.auto_exposure = *settings.auto_exposure;
        }
        if (settings.exposure_us.has_value()) {
            active_settings_.exposure_us = std::clamp<int64_t>(*settings.exposure_us, caps_.min_exposure_us, caps_.max_exposure_us);
        }
        if (settings.auto_gain.has_value()) {
            active_settings_.auto_gain = *settings.auto_gain;
        }
        if (settings.gain.has_value()) {
            active_settings_.gain = std::clamp(*settings.gain, caps_.min_gain, caps_.max_gain);
        }
        if (settings.crop_dslr_pillarbox.has_value()) {
            crop_dslr_pillarbox_ = *settings.crop_dslr_pillarbox;
            active_settings_.crop_dslr_pillarbox = crop_dslr_pillarbox_;
        }

        // Coalesce pending commands so rapid slider movements never backlog hardware I/O
        while (!cmd_queue_.empty()) {
            auto stale = std::move(cmd_queue_.front());
            cmd_queue_.pop();
            if (stale.promise) {
                stale.promise->set_value(core::Result<CameraSettings>::ok(active_settings_));
            }
        }
        cmd_queue_.push(Command{active_settings_, nullptr});
        return core::Result<CameraSettings>::ok(active_settings_);
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    return execute_apply_hardware(settings);
}

core::Result<void> UvcCamera::start(FrameCallback on_frame) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!is_open_) {
        return core::Result<void>::err(core::Error(core::ErrorCode::InvalidState, "Camera not open"));
    }
    if (is_streaming_.load()) {
        return core::Result<void>::ok();
    }

    frame_callback_ = std::move(on_frame);
    is_streaming_.store(true);
    frames_captured_ = 0;
    frames_dropped_hw_ = 0;

    worker_thread_ = std::thread(&UvcCamera::worker_loop, this);
    return core::Result<void>::ok();
}

void UvcCamera::stop() {
    if (is_streaming_.exchange(false)) {
        if (mf_reader_) {
            reinterpret_cast<IMFSourceReader*>(mf_reader_)->Flush(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM));
        }
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
}

bool UvcCamera::try_reconnect() {
    const int delays_sec[] = {1, 2, 4, 8, 8};
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (!is_streaming_.load()) return false;
        SABIT_LOG_INFO("Reconnecting UVC Camera in ", delays_sec[attempt], " s (attempt ", attempt + 1, "/5)...");
        std::this_thread::sleep_for(std::chrono::seconds(delays_sec[attempt]));

        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            release_mf_resources();
        }

        CameraId id{.driver = "uvc", .id = std::to_string(device_index_), .name = device_name_};
        auto res = open(id);
        if (res.is_ok()) {
            SABIT_LOG_INFO("UVC Camera reconnected successfully!");
            execute_apply_hardware(active_settings_);
            is_disconnected_.store(false);
            return true;
        }
    }
    return false;
}

void UvcCamera::worker_loop() {
    IMFSourceReader* pReader = reinterpret_cast<IMFSourceReader*>(mf_reader_);
    if (!pReader) return;

    auto last_fps_time = std::chrono::steady_clock::now();
    uint64_t frames_in_second = 0;
    uint64_t seq = 0;

    const int width = caps_.max_width;
    const int height = caps_.max_height;
    const size_t pixel_count = static_cast<size_t>(width) * height;

    while (is_streaming_.load()) {
        // 1. Process queued commands
        {
            std::unique_lock<std::mutex> lock(cmd_mutex_);
            while (!cmd_queue_.empty()) {
                Command cmd = std::move(cmd_queue_.front());
                cmd_queue_.pop();
                lock.unlock();

                auto res = execute_apply_hardware(cmd.settings);
                if (cmd.promise) {
                    cmd.promise->set_value(res);
                }

                lock.lock();
            }
        }

        // 2. Read frame sample
        DWORD streamIndex = 0;
        DWORD flags = 0;
        LONGLONG llTimestamp = 0;
        IMFSample* pSample = nullptr;

        HRESULT hr = pReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &streamIndex,
            &flags,
            &llTimestamp,
            &pSample
        );

        if (flags & MF_SOURCE_READERF_STREAMTICK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        if (FAILED(hr)) {
            SABIT_LOG_ERROR("UVC ReadSample failed with hr=", std::hex, hr, std::dec);
            is_disconnected_.store(true);
            if (on_disconnected_) {
                on_disconnected_();
            }
            if (auto_reconnect_) {
                if (!try_reconnect()) {
                    is_streaming_.store(false);
                    break;
                }
                pReader = reinterpret_cast<IMFSourceReader*>(mf_reader_);
                continue;
            } else {
                is_streaming_.store(false);
                break;
            }
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            if (SUCCEEDED(pSample->ConvertToContiguousBuffer(&pBuffer)) && pBuffer) {
                BYTE* pData = nullptr;
                DWORD maxLen = 0, currentLen = 0;
                if (SUCCEEDED(pBuffer->Lock(&pData, &maxLen, &currentLen)) && pData) {
                    core::Frame frame = core::Frame::create_allocated(width, height, 3, 16);
                    frame.seq = ++seq;
                    frame.bayer = core::BayerPattern::None;
                    uint16_t* dst = frame.pixels.get();

                    // Compute software WB + fallback exposure/gain multipliers (for virtual webcams like Iriun)
                    float wb_r_mult = 1.0f + (active_settings_.wb_red.value_or(50) - 50) * 0.016f;
                    float wb_b_mult = 1.0f + (active_settings_.wb_blue.value_or(50) - 50) * 0.016f;
                    float exp_gain_mult = 1.0f;

                    if (!active_settings_.auto_gain.value_or(false)) {
                        double g_db = active_settings_.gain.value_or(0.0);
                        if (g_db > 0.1) {
                            float div = hw_gain_supported_ ? 300.0f : 200.0f;
                            exp_gain_mult *= std::pow(10.0f, static_cast<float>(g_db) / div);
                        }
                    }
                    if (!active_settings_.auto_exposure.value_or(false)) {
                        int64_t exp_us = active_settings_.exposure_us.value_or(33333);
                        float ratio = static_cast<float>(exp_us) / 33333.0f;
                        if (!hw_exp_supported_) {
                            exp_gain_mult *= std::clamp(std::sqrt(ratio), 0.12f, 6.0f);
                        } else {
                            exp_gain_mult *= std::clamp(std::pow(ratio, 0.35f), 0.25f, 4.0f);
                        }
                    }

                    const float scale_r = exp_gain_mult * wb_r_mult;
                    const float scale_g = exp_gain_mult;
                    const float scale_b = exp_gain_mult * wb_b_mult;
                    const bool has_color_scale = (std::abs(scale_r - 1.0f) > 0.01f ||
                                                  std::abs(scale_g - 1.0f) > 0.01f ||
                                                  std::abs(scale_b - 1.0f) > 0.01f);

                    auto write_rgb_pixel = [&](size_t px_idx, int r8, int g8, int b8) {
                        if (has_color_scale) {
                            r8 = std::clamp(static_cast<int>(r8 * scale_r + 0.5f), 0, 255);
                            g8 = std::clamp(static_cast<int>(g8 * scale_g + 0.5f), 0, 255);
                            b8 = std::clamp(static_cast<int>(b8 * scale_b + 0.5f), 0, 255);
                        }
                        dst[px_idx * 3 + 0] = static_cast<uint16_t>((r8 << 8) | r8);
                        dst[px_idx * 3 + 1] = static_cast<uint16_t>((g8 << 8) | g8);
                        dst[px_idx * 3 + 2] = static_cast<uint16_t>((b8 << 8) | b8);
                    };

                    const bool flip_v = (default_stride_ < 0);
                    const int abs_stride = (default_stride_ != 0) ? std::abs(default_stride_) : 0;

                    if (video_format_ == 1 || currentLen >= pixel_count * 4) {
                        // RGB32 (BGRA in memory)
                        int row_stride = (abs_stride >= width * 4) ? abs_stride : (width * 4);
                        for (int y = 0; y < height; ++y) {
                            int src_y = flip_v ? (height - 1 - y) : y;
                            const BYTE* row = pData + static_cast<size_t>(src_y) * row_stride;
                            size_t dst_row = static_cast<size_t>(y) * width;
                            for (int x = 0; x < width; ++x) {
                                int b = row[x * 4 + 0];
                                int g = row[x * 4 + 1];
                                int r = row[x * 4 + 2];
                                write_rgb_pixel(dst_row + x, r, g, b);
                            }
                        }
                    } else if (video_format_ == 2 || currentLen >= pixel_count * 3) {
                        // RGB24 (BGR in memory)
                        int row_stride = (abs_stride >= width * 3) ? abs_stride : (width * 3);
                        for (int y = 0; y < height; ++y) {
                            int src_y = flip_v ? (height - 1 - y) : y;
                            const BYTE* row = pData + static_cast<size_t>(src_y) * row_stride;
                            size_t dst_row = static_cast<size_t>(y) * width;
                            for (int x = 0; x < width; ++x) {
                                int b = row[x * 3 + 0];
                                int g = row[x * 3 + 1];
                                int r = row[x * 3 + 2];
                                write_rgb_pixel(dst_row + x, r, g, b);
                            }
                        }
                    } else if (video_format_ == 3 || currentLen >= pixel_count * 2) {
                        // YUY2 (Y0 U0 Y1 V0)
                        for (size_t i = 0; i + 1 < pixel_count; i += 2) {
                            int y0 = pData[i * 2 + 0];
                            int u  = pData[i * 2 + 1] - 128;
                            int y1 = pData[i * 2 + 2];
                            int v  = pData[i * 2 + 3] - 128;
                            auto yuv_to_rgb = [&](int y_val, size_t px_idx) {
                                int r = std::clamp(y_val + ((359 * v) >> 8), 0, 255);
                                int g = std::clamp(y_val - ((88 * u + 183 * v) >> 8), 0, 255);
                                int b = std::clamp(y_val + ((454 * u) >> 8), 0, 255);
                                write_rgb_pixel(px_idx, r, g, b);
                            };
                            yuv_to_rgb(y0, i);
                            yuv_to_rgb(y1, i + 1);
                        }
                    } else if (video_format_ == 4 || currentLen >= (pixel_count * 3) / 2) {
                        // NV12 (Y plane followed by interleaved UV plane)
                        int row_stride = (abs_stride >= width) ? abs_stride : width;
                        const BYTE* y_plane = pData;
                        const BYTE* uv_plane = pData + static_cast<size_t>(row_stride) * height;
                        for (int y = 0; y < height; ++y) {
                            const BYTE* y_row = y_plane + static_cast<size_t>(y) * row_stride;
                            const BYTE* uv_row = uv_plane + static_cast<size_t>(y / 2) * row_stride;
                            size_t dst_row = static_cast<size_t>(y) * width;
                            for (int x = 0; x < width; ++x) {
                                int y_val = y_row[x];
                                int u = uv_row[(x & ~1) + 0] - 128;
                                int v = uv_row[(x & ~1) + 1] - 128;
                                int r = std::clamp(y_val + ((359 * v) >> 8), 0, 255);
                                int g = std::clamp(y_val - ((88 * u + 183 * v) >> 8), 0, 255);
                                int b = std::clamp(y_val + ((454 * u) >> 8), 0, 255);
                                write_rgb_pixel(dst_row + x, r, g, b);
                            }
                        }
                    }

                    pBuffer->Unlock();

                    frame.exposure_us = active_settings_.exposure_us.value_or(33333);
                    frame.gain = active_settings_.gain.value_or(0.0);
                    frame.binning = 1;
                    frame.timestamp_utc = core::Clock::now_utc();
                    frame.clock_sync = core::Clock::get_sync();

                    ++frames_captured_;
                    ++frames_in_second;

                    // Auto-Crop ROI / Aspect Ratio: Crop black pillarbox bars from 16:9 HDMI output (e.g. 1920x1080 -> 1620x1080) for 3:2 DSLR sensors
                    if (crop_dslr_pillarbox_ && width > 100 && height > 100) {
                        int target_w = static_cast<int>(std::round(height * 1.5));
                        target_w -= (target_w % 2); // ensure even width
                        if (target_w < width) {
                            int crop_x = (width - target_w) / 2;
                            crop_x -= (crop_x % 2); // ensure even offset
                            int crop_w = target_w;
                            int crop_h = height;

                            core::Frame cropped = core::Frame::create_allocated(crop_w, crop_h, 3, 16);
                            cropped.seq = frame.seq;
                            cropped.bayer = frame.bayer;
                            cropped.exposure_us = frame.exposure_us;
                            cropped.gain = frame.gain;
                            cropped.binning = frame.binning;
                            cropped.timestamp_utc = frame.timestamp_utc;
                            cropped.clock_sync = frame.clock_sync;

                            const uint16_t* src_ptr = frame.pixels.get();
                            uint16_t* dst_ptr = cropped.pixels.get();

                            for (int y = 0; y < crop_h; ++y) {
                                const uint16_t* s_row = src_ptr + (static_cast<size_t>(y) * width + crop_x) * 3;
                                uint16_t* d_row = dst_ptr + static_cast<size_t>(y) * crop_w * 3;
                                std::memcpy(d_row, s_row, static_cast<size_t>(crop_w) * 3 * sizeof(uint16_t));
                            }
                            frame = std::move(cropped);
                        }
                    }

                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_time).count();
                    if (elapsed >= 1000) {
                        actual_fps_ = (static_cast<double>(frames_in_second) * 1000.0) / elapsed;
                        frames_in_second = 0;
                        last_fps_time = now;
                    }

                    if (frame_callback_) {
                        frame_callback_(frame);
                    }
                }
                pBuffer->Release();
            }
            pSample->Release();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

} // namespace sabit::camera
