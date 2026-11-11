#include "src/camera/ascom/ascom_camera.h"
#include "src/core/clock.h"
#include "src/core/logger.h"

#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <set>

namespace sabit::camera {

namespace {

std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wstr(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr.data(), len);
    return wstr;
}

HRESULT com_get_dispid(IDispatch* disp, const wchar_t* name, DISPID* out_id) {
    if (!disp || !name || !out_id) return E_INVALIDARG;
    LPOLESTR names[1] = {const_cast<LPOLESTR>(name)};
    return disp->GetIDsOfNames(IID_NULL, names, 1, LOCALE_USER_DEFAULT, out_id);
}

HRESULT com_get_property(IDispatch* disp, const wchar_t* name, VARIANT* out_result) {
    if (!disp || !out_result) return E_INVALIDARG;
    VariantInit(out_result);
    DISPID dispid = 0;
    HRESULT hr = com_get_dispid(disp, name, &dispid);
    if (FAILED(hr)) return hr;

    DISPPARAMS params = {nullptr, nullptr, 0, 0};
    return disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, out_result, nullptr, nullptr);
}

HRESULT com_put_property(IDispatch* disp, const wchar_t* name, VARIANT val) {
    if (!disp) return E_INVALIDARG;
    DISPID dispid = 0;
    HRESULT hr = com_get_dispid(disp, name, &dispid);
    if (FAILED(hr)) return hr;

    DISPID propput_id = DISPID_PROPERTYPUT;
    DISPPARAMS params;
    params.rgvarg = &val;
    params.rgdispidNamedArgs = &propput_id;
    params.cArgs = 1;
    params.cNamedArgs = 1;

    return disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT, &params, nullptr, nullptr, nullptr);
}

bool com_get_bool(IDispatch* disp, const wchar_t* name, bool default_val = false) {
    VARIANT v;
    if (SUCCEEDED(com_get_property(disp, name, &v))) {
        bool res = default_val;
        if (v.vt == VT_BOOL) res = (v.boolVal != VARIANT_FALSE);
        else if (v.vt == VT_I4) res = (v.lVal != 0);
        else if (v.vt == VT_I2) res = (v.iVal != 0);
        VariantClear(&v);
        return res;
    }
    return default_val;
}

long com_get_long(IDispatch* disp, const wchar_t* name, long default_val = 0) {
    VARIANT v;
    if (SUCCEEDED(com_get_property(disp, name, &v))) {
        VARIANT converted;
        VariantInit(&converted);
        long res = default_val;
        if (SUCCEEDED(VariantChangeType(&converted, &v, 0, VT_I4))) {
            res = converted.lVal;
        }
        VariantClear(&converted);
        VariantClear(&v);
        return res;
    }
    return default_val;
}

double com_get_double(IDispatch* disp, const wchar_t* name, double default_val = 0.0) {
    VARIANT v;
    if (SUCCEEDED(com_get_property(disp, name, &v))) {
        VARIANT converted;
        VariantInit(&converted);
        double res = default_val;
        if (SUCCEEDED(VariantChangeType(&converted, &v, 0, VT_R8))) {
            res = converted.dblVal;
        }
        VariantClear(&converted);
        VariantClear(&v);
        return res;
    }
    return default_val;
}

HRESULT com_put_bool(IDispatch* disp, const wchar_t* name, bool val) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_BOOL;
    v.boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
    return com_put_property(disp, name, v);
}

HRESULT com_put_short(IDispatch* disp, const wchar_t* name, short val) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I2;
    v.iVal = val;
    return com_put_property(disp, name, v);
}

HRESULT com_put_long(IDispatch* disp, const wchar_t* name, long val) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = val;
    return com_put_property(disp, name, v);
}

HRESULT com_put_double(IDispatch* disp, const wchar_t* name, double val) {
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_R8;
    v.dblVal = val;
    return com_put_property(disp, name, v);
}

HRESULT com_call_start_exposure(IDispatch* disp, double duration_sec, bool light_frame = true) {
    if (!disp) return E_INVALIDARG;
    DISPID dispid = 0;
    HRESULT hr = com_get_dispid(disp, L"StartExposure", &dispid);
    if (FAILED(hr)) return hr;

    // Note: IDispatch arguments are passed in reverse order!
    VARIANT args[2];
    VariantInit(&args[0]);
    args[0].vt = VT_BOOL;
    args[0].boolVal = light_frame ? VARIANT_TRUE : VARIANT_FALSE;

    VariantInit(&args[1]);
    args[1].vt = VT_R8;
    args[1].dblVal = duration_sec;

    DISPPARAMS params;
    params.rgvarg = args;
    params.rgdispidNamedArgs = nullptr;
    params.cArgs = 2;
    params.cNamedArgs = 0;

    return disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
}

HRESULT com_call_void_method(IDispatch* disp, const wchar_t* name) {
    if (!disp || !name) return E_INVALIDARG;
    DISPID dispid = 0;
    HRESULT hr = com_get_dispid(disp, name, &dispid);
    if (FAILED(hr)) return hr;

    DISPPARAMS params = {nullptr, nullptr, 0, 0};
    return disp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, nullptr, nullptr, nullptr);
}

// Pump pending STA Windows messages on the COM thread (required by DSLR EDSDK/Nikon/WIA and LocalServer ASCOM drivers)
void pump_sta_messages() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void enumerate_registry_root(HKEY root, const char* subkey_path, REGSAM sam_flags, std::vector<CameraId>& out_list, std::set<std::string>& seen) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(root, subkey_path, 0, KEY_READ | sam_flags, &hKey) != ERROR_SUCCESS || !hKey) {
        return;
    }

    char subkey_name[256];
    DWORD index = 0;
    DWORD name_len = sizeof(subkey_name);

    while (RegEnumKeyExA(hKey, index, subkey_name, &name_len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
        std::string prog_id(subkey_name);
        if (!prog_id.empty() && seen.find(prog_id) == seen.end()) {
            HKEY hSub = nullptr;
            std::string friendly = prog_id;
            if (RegOpenKeyExA(hKey, subkey_name, 0, KEY_READ | sam_flags, &hSub) == ERROR_SUCCESS && hSub) {
                char val_buf[256] = {0};
                DWORD val_size = sizeof(val_buf) - 1;
                DWORD type = 0;
                if (RegQueryValueExA(hSub, nullptr, nullptr, &type, reinterpret_cast<LPBYTE>(val_buf), &val_size) == ERROR_SUCCESS &&
                    (type == REG_SZ || type == REG_EXPAND_SZ) && val_buf[0] != '\0') {
                    friendly = val_buf;
                }
                RegCloseKey(hSub);
            }
            seen.insert(prog_id);
            out_list.push_back(CameraId{
                .driver = "ascom",
                .id = prog_id,
                .name = "ASCOM: " + friendly
            });
        }
        ++index;
        name_len = sizeof(subkey_name);
    }

    RegCloseKey(hKey);
}

std::atomic<int> g_connected_ascom_count{0};

} // namespace

uint16_t scale_to_u16(int32_t value, int32_t max_adu) {
    if (value <= 0) {
        return 0;
    }
    if (max_adu > 0 && max_adu <= 255) {
        int32_t clamped = std::min(value, max_adu);
        return static_cast<uint16_t>((static_cast<int64_t>(clamped) * 65535) / max_adu);
    }
    return static_cast<uint16_t>(std::clamp<int32_t>(value, 0, 65535));
}

AscomCamera::AscomCamera() = default;

AscomCamera::~AscomCamera() {
    close();
}

core::Result<void> AscomCamera::show_driver_setup(const std::string& prog_id) {
    if (g_connected_ascom_count.load() > 0) {
        return core::Result<void>::err(
            core::ErrorCode::Busy,
            "Cannot open ASCOM SetupDialog while camera is connected"
        );
    }

    std::string clean_id = prog_id;
    if (clean_id.rfind("ASCOM:", 0) == 0) {
        clean_id = clean_id.substr(6);
    }
    if (clean_id.empty()) {
        return core::Result<void>::err(
            core::ErrorCode::NotFound,
            "ASCOM ProgID is empty"
        );
    }

    auto setup_promise = std::make_shared<std::promise<core::Result<void>>>();
    auto setup_future = setup_promise->get_future();

    std::thread sta_thread([clean_id, setup_promise]() {
        HRESULT hr_init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        bool should_uninit = SUCCEEDED(hr_init);

        CLSID clsid{};
        std::wstring w_prog_id = utf8_to_wide(clean_id);
        if (FAILED(CLSIDFromProgID(w_prog_id.c_str(), &clsid))) {
            setup_promise->set_value(core::Result<void>::err(
                core::ErrorCode::DriverMissing,
                "ASCOM ProgID not registered on this system: " + clean_id
            ));
            if (should_uninit) CoUninitialize();
            return;
        }

        IDispatch* disp = nullptr;
        HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&disp));
        pump_sta_messages();
        if (FAILED(hr) || !disp) {
            setup_promise->set_value(core::Result<void>::err(
                core::ErrorCode::HardwareError,
                "Failed to instantiate ASCOM driver COM object: " + clean_id
            ));
            if (should_uninit) CoUninitialize();
            return;
        }

        HRESULT hr_setup = com_call_void_method(disp, L"SetupDialog");
        pump_sta_messages();
        disp->Release();
        if (should_uninit) CoUninitialize();

        if (FAILED(hr_setup)) {
            setup_promise->set_value(core::Result<void>::err(
                core::ErrorCode::HardwareError,
                "ASCOM SetupDialog call failed for: " + clean_id
            ));
            return;
        }

        setup_promise->set_value(core::Result<void>::ok());
    });

    while (setup_future.wait_for(std::chrono::milliseconds(15)) != std::future_status::ready) {
        pump_sta_messages();
    }
    if (sta_thread.joinable()) {
        sta_thread.join();
    }
    return setup_future.get();
}

std::vector<CameraId> AscomCamera::enumerate_cameras(bool include_fallback_defaults) {
    std::vector<CameraId> list;
    std::set<std::string> seen;

    enumerate_registry_root(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASCOM\\Camera Drivers", KEY_WOW64_64KEY, list, seen);
    enumerate_registry_root(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASCOM\\Camera Drivers", KEY_WOW64_32KEY, list, seen);
    enumerate_registry_root(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\ASCOM\\Camera Drivers", 0, list, seen);
    enumerate_registry_root(HKEY_CURRENT_USER,  "Software\\ASCOM\\Camera Drivers", 0, list, seen);

    if (list.empty() && include_fallback_defaults) {
        // Match Onyx OS get_ascom_cameras() default ProgIDs when ASCOM Platform registry entries are not populated
        const std::pair<const char*, const char*> defaults[] = {
            {"ASCOM.ASICamera1.Camera", "ASCOM: ASI Camera (1)"},
            {"ASCOM.ASICamera2.Camera", "ASCOM: ASI Camera (2)"},
            {"ASCOM.Simulator.Camera",  "ASCOM: OmniSim / V3 Simulator"},
            {"ASCOM.QHYCCD.Camera",     "ASCOM: QHYCCD-Cameras-Capture"}
        };
        for (const auto& [pid, name] : defaults) {
            list.push_back(CameraId{
                .driver = "ascom",
                .id = pid,
                .name = name
            });
        }
    }

    return list;
}

core::Result<CameraCaps> AscomCamera::open(const CameraId& id) {
    close();

    std::unique_lock<std::mutex> lock(state_mutex_);
    prog_id_ = id.id;
    // Strip "ASCOM:" prefix if present
    if (prog_id_.rfind("ASCOM:", 0) == 0) {
        prog_id_ = prog_id_.substr(6);
    }
    display_name_ = id.name.empty() ? ("ASCOM " + prog_id_) : id.name;

    active_settings_.exposure_us = 100000; // 100 ms (0.1 s)
    active_settings_.gain = 100.0;
    active_settings_.wb_red = 50;
    active_settings_.wb_blue = 50;
    active_settings_.binning = 1;
    active_settings_.bit_depth = 16;

    auto open_promise = std::make_shared<std::promise<core::Result<CameraCaps>>>();
    auto open_future = open_promise->get_future();

    worker_running_.store(true);
    is_streaming_.store(false);
    is_disconnected_.store(false);

    worker_thread_ = std::thread(&AscomCamera::camera_worker_loop, this, open_promise);
    lock.unlock();

    // Wait for the dedicated STA worker thread to connect once (pumping Windows messages so UI never shows "Not Responding")
    auto start_wait = std::chrono::steady_clock::now();
    while (open_future.wait_for(std::chrono::milliseconds(15)) != std::future_status::ready) {
        pump_sta_messages();
        auto elapsed_s = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_wait).count();
        if (elapsed_s >= 15) {
            worker_running_.store(false);
            if (worker_thread_.joinable()) {
                worker_thread_.detach();
            }
            return core::Result<CameraCaps>::err(
                core::ErrorCode::Timeout,
                "ASCOM DSLR/CCD driver connection timed out: " + prog_id_
            );
        }
    }

    auto res = open_future.get();
    if (!res.is_ok()) {
        worker_running_.store(false);
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        return res;
    }

    SABIT_LOG_INFO("Connected ASCOM Camera (Single Persistent STA Session): ", caps_.model, " (", caps_.max_width, "x", caps_.max_height, ")");
    return res;
}

void AscomCamera::close() {
    is_streaming_.store(false);
    worker_running_.store(false);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (is_open_) {
        is_open_ = false;
        g_connected_ascom_count.fetch_sub(1);
    }
}

CameraCaps AscomCamera::caps() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return caps_;
}

CameraStatus AscomCamera::status() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    CameraStatus s{};
    s.is_connected = is_open_ && !is_disconnected_.load();
    s.is_streaming = is_streaming_.load();
    s.actual_fps = actual_fps_;
    s.frames_captured = frames_captured_;
    s.frames_dropped_hw = frames_dropped_hw_;
    s.sensor_temp_c = sensor_temp_c_;
    s.cooler_power_percent = cooler_power_pct_;
    return s;
}

core::Result<CameraSettings> AscomCamera::apply(const CameraSettings& settings) {
    if (!is_open_) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "ASCOM Camera is not open");
    }

    {
        std::lock_guard<std::mutex> lock(cmd_mutex_);
        if (settings.exposure_us.has_value()) {
            active_settings_.exposure_us = std::clamp<int64_t>(*settings.exposure_us, 100, 3600000000LL);
        }
        if (settings.gain.has_value()) {
            active_settings_.gain = std::clamp(*settings.gain, 0.0, 1000.0);
        }
        if (settings.wb_red.has_value()) {
            active_settings_.wb_red = std::clamp(*settings.wb_red, 0, 100);
        }
        if (settings.wb_blue.has_value()) {
            active_settings_.wb_blue = std::clamp(*settings.wb_blue, 0, 100);
        }
        if (settings.auto_exposure.has_value()) {
            active_settings_.auto_exposure = *settings.auto_exposure;
        }
        if (settings.auto_gain.has_value()) {
            active_settings_.auto_gain = *settings.auto_gain;
        }
        if (settings.bit_depth.has_value()) {
            active_settings_.bit_depth = (*settings.bit_depth == 8) ? 8 : 16;
        }

        // Coalesce pending commands so rapid slider drags never queue up or block the UI thread
        while (!cmd_queue_.empty()) {
            auto stale = std::move(cmd_queue_.front());
            cmd_queue_.pop();
            if (stale.promise) {
                stale.promise->set_value(core::Result<CameraSettings>::ok(active_settings_));
            }
        }
        cmd_queue_.push(Command{settings, nullptr});
        CameraSettings ret = active_settings_;
        if (settings.binning.has_value()) ret.binning = std::clamp(*settings.binning, 1, 4);
        if (settings.cooler.has_value()) ret.cooler = settings.cooler;
        return core::Result<CameraSettings>::ok(ret);
    }
}

core::Result<CameraSettings> AscomCamera::execute_apply_com(void* disp_ptr, const CameraSettings& req) {
    IDispatch* disp = reinterpret_cast<IDispatch*>(disp_ptr);
    if (!disp) {
        return core::Result<CameraSettings>::err(core::ErrorCode::InvalidState, "ASCOM IDispatch null");
    }

    if (req.exposure_us.has_value()) {
        int64_t exp = *req.exposure_us;
        if (exp < 1 || exp > 3600000000LL) {
            return core::Result<CameraSettings>::err(core::ErrorCode::OutOfRange, "Exposure out of range");
        }
        active_settings_.exposure_us = exp;
    }

    if (req.gain.has_value()) {
        if (caps_.capabilities.can_control_gain) {
            short g = static_cast<short>(std::clamp(*req.gain, 0.0, 1000.0));
            com_put_short(disp, L"Gain", g);
        }
        active_settings_.gain = *req.gain;
    }

    if (req.binning.has_value()) {
        short bin = static_cast<short>(std::clamp(*req.binning, 1, 4));
        if (!active_settings_.binning.has_value() || *active_settings_.binning != bin) {
            com_put_short(disp, L"BinX", bin);
            com_put_short(disp, L"BinY", bin);
            long num_x = std::max<long>(1, caps_.max_width / bin);
            long num_y = std::max<long>(1, caps_.max_height / bin);
            com_put_long(disp, L"StartX", 0);
            com_put_long(disp, L"StartY", 0);
            com_put_long(disp, L"NumX", num_x);
            com_put_long(disp, L"NumY", num_y);
            active_settings_.binning = bin;
        }
    }

    if (req.bit_depth.has_value()) {
        active_settings_.bit_depth = (*req.bit_depth == 8) ? 8 : 16;
    }

    if (req.cooler.has_value()) {
        const auto& c = *req.cooler;
        com_put_bool(disp, L"CoolerOn", c.enabled);
        if (c.enabled && c.target_c.has_value()) {
            com_put_double(disp, L"SetCCDTemperature", *c.target_c);
        }
        active_settings_.cooler = c;
    }

    return core::Result<CameraSettings>::ok(active_settings_);
}

core::Result<void> AscomCamera::start(FrameCallback on_frame) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!is_open_ || !worker_running_.load()) {
        return core::Result<void>::err(core::ErrorCode::InvalidState, "ASCOM Camera is not open");
    }
    frame_callback_ = std::move(on_frame);
    frames_captured_ = 0;
    frames_dropped_hw_ = 0;
    is_streaming_.store(true);
    return core::Result<void>::ok();
}

void AscomCamera::stop() {
    is_streaming_.store(false);
}

void AscomCamera::camera_worker_loop(std::shared_ptr<std::promise<core::Result<CameraCaps>>> open_promise) {
    // Initialize COM in STA mode on the dedicated worker thread (required by ASCOM DSLR & CCD drivers)
    HRESULT hr_init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool should_uninit = SUCCEEDED(hr_init);

    CLSID clsid{};
    std::wstring w_prog_id = utf8_to_wide(prog_id_);
    if (FAILED(CLSIDFromProgID(w_prog_id.c_str(), &clsid))) {
        worker_running_.store(false);
        if (open_promise) {
            open_promise->set_value(core::Result<CameraCaps>::err(
                core::ErrorCode::DriverMissing,
                "ASCOM ProgID not registered on this system: " + prog_id_
            ));
        }
        if (should_uninit) CoUninitialize();
        return;
    }

    IDispatch* disp = nullptr;
    HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&disp));
    pump_sta_messages();
    if (FAILED(hr) || !disp) {
        worker_running_.store(false);
        if (open_promise) {
            open_promise->set_value(core::Result<CameraCaps>::err(
                core::ErrorCode::HardwareError,
                "Failed to instantiate ASCOM driver COM object: " + prog_id_
            ));
        }
        if (should_uninit) CoUninitialize();
        return;
    }

    hr = com_put_bool(disp, L"Connected", true);
    pump_sta_messages();
    if (FAILED(hr) || !com_get_bool(disp, L"Connected", false)) {
        disp->Release();
        worker_running_.store(false);
        if (open_promise) {
            open_promise->set_value(core::Result<CameraCaps>::err(
                core::ErrorCode::HardwareError,
                "ASCOM driver refused connection (Connected = True failed): " + prog_id_
            ));
        }
        if (should_uninit) CoUninitialize();
        return;
    }

    // Enable FastReadout if supported by CCD/CMOS/DSLR driver for smoother live preview
    com_put_bool(disp, L"FastReadout", true);
    pump_sta_messages();

    long cam_x = com_get_long(disp, L"CameraXSize", 1920);
    long cam_y = com_get_long(disp, L"CameraYSize", 1080);
    long max_bin_x = com_get_long(disp, L"MaxBinX", 4);
    bool can_set_temp = com_get_bool(disp, L"CanSetCCDTemperature", false);
    can_abort_exposure_ = com_get_bool(disp, L"CanAbortExposure", false);
    long sensor_type = com_get_long(disp, L"SensorType", 0); // 0=Mono, 1=Color, 2=RGGB
    long raw_max_adu = com_get_long(disp, L"MaxADU", 65535);
    max_adu_ = (raw_max_adu > 0) ? static_cast<int32_t>(raw_max_adu) : 65535;
    const bool is_live_view_adu = (max_adu_ <= 255);
    long gain_min = com_get_long(disp, L"GainMin", 0);
    long gain_max = com_get_long(disp, L"GainMax", 400);
    if (gain_max <= gain_min) {
        gain_min = 0;
        gain_max = 400;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        caps_.model = display_name_;
        caps_.serial = prog_id_;
        caps_.driver = "ascom";
        caps_.sdk_version = "ASCOM COM Camera V2/V3";
        caps_.max_width = (cam_x > 0) ? static_cast<int>(cam_x) : 1920;
        caps_.max_height = (cam_y > 0) ? static_cast<int>(cam_y) : 1080;
        caps_.min_exposure_us = 1000;          // 1 ms
        caps_.max_exposure_us = 3600000000LL;  // 3600 s (1 hour)
        caps_.min_gain = static_cast<double>(gain_min);
        caps_.max_gain = static_cast<double>(gain_max);
        caps_.has_cooler = can_set_temp;
        caps_.is_color = (sensor_type != 0);
        caps_.bayer = (sensor_type >= 2) ? core::BayerPattern::RGGB : core::BayerPattern::None;

        caps_.supported_bins.clear();
        for (int b = 1; b <= std::clamp<int>(static_cast<int>(max_bin_x), 1, 4); ++b) {
            caps_.supported_bins.push_back(b);
        }
        caps_.supported_bit_depths = {8, 16};

        caps_.capabilities.can_control_exposure = true;
        caps_.capabilities.can_control_gain = true;
        caps_.capabilities.can_control_wb = caps_.is_color;
        caps_.capabilities.can_control_roi = true;
        caps_.capabilities.can_control_pixel_format = true;
        caps_.capabilities.has_hardware_exposure = !is_live_view_adu;
        caps_.capabilities.has_hardware_gain = !is_live_view_adu;
        caps_.capabilities.has_software_fallback = is_live_view_adu;
        caps_.capabilities.can_auto_exposure = false;
        caps_.capabilities.can_auto_gain = false;
        caps_.capabilities.can_crop_dslr_aspect = is_live_view_adu;

        if (!is_open_) {
            is_open_ = true;
            g_connected_ascom_count.fetch_add(1);
        }
    }

    // Apply initial settings once on the connected COM object
    CameraSettings init_req = active_settings_;
    active_settings_.binning = std::nullopt;
    execute_apply_com(disp, init_req);
    pump_sta_messages();

    if (open_promise) {
        open_promise->set_value(core::Result<CameraCaps>::ok(caps_));
        open_promise.reset();
    }

    uint64_t seq = 0;
    auto last_fps_time = std::chrono::steady_clock::now();
    auto last_temp_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    auto last_warn_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    uint64_t suppressed_warns = 0;
    uint64_t frames_in_second = 0;

    while (worker_running_.load()) {
        pump_sta_messages();

        // 1. Apply pending hardware commands on the COM thread
        {
            std::unique_lock<std::mutex> lock(cmd_mutex_);
            while (!cmd_queue_.empty()) {
                auto cmd = std::move(cmd_queue_.front());
                cmd_queue_.pop();
                lock.unlock();

                auto res = execute_apply_com(disp, cmd.settings);
                pump_sta_messages();
                if (cmd.promise) {
                    cmd.promise->set_value(res);
                }
                lock.lock();
            }
        }

        if (!is_streaming_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            continue;
        }

        double exp_sec = static_cast<double>(active_settings_.exposure_us.value_or(100000)) / 1e6;
        if (is_live_view_adu) {
            exp_sec = 0.001; // minimal dummy duration for ASCOM.DSLR live view frame grab
        } else {
            exp_sec = std::max(0.001, exp_sec);
        }

        if (FAILED(com_call_start_exposure(disp, exp_sec, true))) {
            auto now_warn = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now_warn - last_warn_time).count() >= 5) {
                SABIT_LOG_WARN("[ASCOM] StartExposure failed on: ", prog_id_, " (suppressed: ", suppressed_warns, ")");
                last_warn_time = now_warn;
                suppressed_warns = 0;
            } else {
                ++suppressed_warns;
            }
            pump_sta_messages();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // Wait until ImageReady is True, pumping STA messages and watching for abort/settings changes
        auto exp_start = std::chrono::steady_clock::now();
        const double max_wait_s = std::max(6.0, exp_sec * 2.5 + 8.0);
        bool exposure_aborted = false;

        while (worker_running_.load() && is_streaming_.load()) {
            pump_sta_messages();
            if (com_get_bool(disp, L"ImageReady", false)) {
                break;
            }

            // If user changed settings during a long DSLR/CCD exposure (> 300ms), apply immediately
            {
                std::unique_lock<std::mutex> lock(cmd_mutex_);
                if (!cmd_queue_.empty()) {
                    auto cmd = std::move(cmd_queue_.front());
                    cmd_queue_.pop();
                    lock.unlock();

                    double prev_exp_s = exp_sec;
                    auto res = execute_apply_com(disp, cmd.settings);
                    if (cmd.promise) {
                        cmd.promise->set_value(res);
                    }
                    double new_exp_s = static_cast<double>(active_settings_.exposure_us.value_or(100000)) / 1e6;
                    if (prev_exp_s > 0.35 && new_exp_s < prev_exp_s * 0.6 && can_abort_exposure_) {
                        com_call_void_method(disp, L"AbortExposure");
                        pump_sta_messages();
                        exposure_aborted = true;
                        break;
                    }
                }
            }

            auto waited_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - exp_start).count();
            if (waited_s > max_wait_s) {
                auto now_warn = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now_warn - last_warn_time).count() >= 5) {
                    SABIT_LOG_WARN("[ASCOM] Exposure wait timed out after ", waited_s, "s on: ", prog_id_, " (suppressed: ", suppressed_warns, ")");
                    last_warn_time = now_warn;
                    suppressed_warns = 0;
                } else {
                    ++suppressed_warns;
                }
                if (can_abort_exposure_) {
                    com_call_void_method(disp, L"AbortExposure");
                }
                exposure_aborted = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!worker_running_.load() || !is_streaming_.load()) {
            if (can_abort_exposure_) {
                com_call_void_method(disp, L"AbortExposure");
                pump_sta_messages();
            }
            continue;
        }
        if (exposure_aborted) {
            continue;
        }

        // Retrieve ImageArray SAFEARRAY
        VARIANT var_img;
        if (SUCCEEDED(com_get_property(disp, L"ImageArray", &var_img))) {
            SAFEARRAY* psa = nullptr;
            if ((var_img.vt & VT_ARRAY) && var_img.parray) {
                psa = var_img.parray;
            } else if (var_img.vt == (VT_BYREF | VT_VARIANT) && var_img.pvarVal && (var_img.pvarVal->vt & VT_ARRAY)) {
                psa = var_img.pvarVal->parray;
            }

            if (psa) {
                UINT dims = SafeArrayGetDim(psa);
                if (dims >= 2) {
                    long lb1 = 0, ub1 = -1, lb2 = 0, ub2 = -1, lb3 = 0, ub3 = -1;
                    SafeArrayGetLBound(psa, 1, &lb1);
                    SafeArrayGetUBound(psa, 1, &ub1);
                    SafeArrayGetLBound(psa, 2, &lb2);
                    SafeArrayGetUBound(psa, 2, &ub2);
                    if (dims >= 3) {
                        SafeArrayGetLBound(psa, 3, &lb3);
                        SafeArrayGetUBound(psa, 3, &ub3);
                    }

                    int d1 = static_cast<int>(ub1 - lb1 + 1);
                    int d2 = static_cast<int>(ub2 - lb2 + 1);
                    int d3 = (dims >= 3) ? static_cast<int>(ub3 - lb3 + 1) : 1;

                    // Determine width, height, and channels (handles both [W][H], [W][H][3], and [3][W][H] DSLR arrays)
                    int w = d1;
                    int h = d2;
                    int ch = 1;
                    bool plane_major_rgb = false; // true if [W][H][3], false if [3][W][H]
                    if (dims >= 3) {
                        if (d3 == 3) {
                            w = d1;
                            h = d2;
                            ch = 3;
                            plane_major_rgb = true;
                        } else if (d1 == 3) {
                            ch = 3;
                            w = d2;
                            h = d3;
                            plane_major_rgb = false;
                        }
                    }

                    if (w > 0 && h > 0) {
                        void* raw_ptr = nullptr;
                        if (SUCCEEDED(SafeArrayAccessData(psa, &raw_ptr)) && raw_ptr) {
                            int bd = active_settings_.bit_depth.value_or(16);
                            core::Frame frame = core::Frame::create_allocated(w, h, ch, bd);
                            frame.seq = ++seq;
                            frame.bayer = (ch == 3) ? core::BayerPattern::None : caps_.bayer;

                            VARTYPE elem_vt = VT_EMPTY;
                            SafeArrayGetVartype(psa, &elem_vt);
                            if (elem_vt == VT_EMPTY) {
                                elem_vt = static_cast<VARTYPE>(var_img.vt & ~VT_ARRAY & ~VT_BYREF);
                            }
                            uint16_t* dst = frame.pixels.get();

                            const size_t pixel_count = static_cast<size_t>(w) * static_cast<size_t>(h);
                            const size_t total_samples = pixel_count * static_cast<size_t>(ch);
                            const int32_t sample_max_adu = (elem_vt == VT_UI1 || elem_vt == VT_I1 || (elem_vt == VT_EMPTY && psa->cbElements == 1))
                                ? std::min<int32_t>(max_adu_, 255)
                                : max_adu_;

                            auto read_sample = [&](size_t idx) -> uint16_t {
                                int32_t v = 0;
                                if (elem_vt == VT_I4 || elem_vt == VT_INT || elem_vt == VT_UI4 || (elem_vt == VT_EMPTY && psa->cbElements == 4)) {
                                    v = static_cast<const int32_t*>(raw_ptr)[idx];
                                } else if (elem_vt == VT_UI2 || (elem_vt == VT_EMPTY && psa->cbElements == 2)) {
                                    v = static_cast<int32_t>(static_cast<const uint16_t*>(raw_ptr)[idx]);
                                } else if (elem_vt == VT_I2) {
                                    v = static_cast<int32_t>(static_cast<const int16_t*>(raw_ptr)[idx]);
                                } else if (elem_vt == VT_UI1 || elem_vt == VT_I1 || (elem_vt == VT_EMPTY && psa->cbElements == 1)) {
                                    v = static_cast<int32_t>(static_cast<const uint8_t*>(raw_ptr)[idx]);
                                } else if (elem_vt == VT_R8) {
                                    v = static_cast<int32_t>(std::lround(static_cast<const double*>(raw_ptr)[idx]));
                                } else if (elem_vt == VT_VARIANT) {
                                    const VARIANT& var = static_cast<const VARIANT*>(raw_ptr)[idx];
                                    if (var.vt == VT_I4 || var.vt == VT_INT) v = var.lVal;
                                    else if (var.vt == VT_I2) v = var.iVal;
                                    else if (var.vt == VT_UI2) v = var.uiVal;
                                    else if (var.vt == VT_UI1) v = var.bVal;
                                    else if (var.vt == VT_R8) v = static_cast<int32_t>(std::lround(var.dblVal));
                                }
                                return scale_to_u16(v, sample_max_adu);
                            };

                            float trim_mult = 1.0f;
                            if (is_live_view_adu) {
                                int64_t exp_us = active_settings_.exposure_us.value_or(33333);
                                float ratio = static_cast<float>(exp_us) / 33333.0f;
                                trim_mult *= std::clamp(std::sqrt(ratio), 0.15f, 6.0f);
                                double g_db = active_settings_.gain.value_or(0.0);
                                if (g_db > 0.0) {
                                    trim_mult *= std::pow(10.0f, static_cast<float>(g_db) / 200.0f);
                                }
                            }

                            if (ch == 1) {
                                if (sample_max_adu > 255 && (elem_vt == VT_UI2 || elem_vt == VT_I2 || (elem_vt == VT_EMPTY && psa->cbElements == 2))) {
                                    std::memcpy(dst, raw_ptr, total_samples * sizeof(uint16_t));
                                } else {
                                    for (size_t i = 0; i < total_samples; ++i) {
                                        uint16_t s = read_sample(i);
                                        if (trim_mult != 1.0f) {
                                            s = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(s * trim_mult), 0, 65535));
                                        }
                                        dst[i] = s;
                                    }
                                }
                            } else {
                                // 3-channel RGB DSLR frame (apply software WB & software exposure trim multipliers if adjusted)
                                float wb_r = 1.0f + (active_settings_.wb_red.value_or(50) - 50) * 0.015f;
                                float wb_b = 1.0f + (active_settings_.wb_blue.value_or(50) - 50) * 0.015f;
                                for (size_t i = 0; i < pixel_count; ++i) {
                                    uint16_t r_val = 0, g_val = 0, b_val = 0;
                                    if (plane_major_rgb) {
                                        r_val = read_sample(i);
                                        g_val = read_sample(pixel_count + i);
                                        b_val = read_sample(pixel_count * 2 + i);
                                    } else {
                                        r_val = read_sample(i * 3 + 0);
                                        g_val = read_sample(i * 3 + 1);
                                        b_val = read_sample(i * 3 + 2);
                                    }
                                    if (trim_mult != 1.0f) {
                                        r_val = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(r_val * trim_mult), 0, 65535));
                                        g_val = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(g_val * trim_mult), 0, 65535));
                                        b_val = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(b_val * trim_mult), 0, 65535));
                                    }
                                    dst[i * 3 + 0] = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(r_val * wb_r), 0, 65535));
                                    dst[i * 3 + 1] = g_val;
                                    dst[i * 3 + 2] = static_cast<uint16_t>(std::clamp<int>(static_cast<int>(b_val * wb_b), 0, 65535));
                                }
                            }

                            SafeArrayUnaccessData(psa);

                            auto now = std::chrono::steady_clock::now();
                            if (caps_.has_cooler && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_temp_time).count() >= 2000) {
                                sensor_temp_c_ = com_get_double(disp, L"CCDTemperature", sensor_temp_c_);
                                cooler_power_pct_ = static_cast<int>(com_get_double(disp, L"CoolerPower", 0.0));
                                last_temp_time = now;
                            }

                            frame.sensor_temp_c = sensor_temp_c_;
                            frame.exposure_us = active_settings_.exposure_us.value_or(100000);
                            frame.gain = active_settings_.gain.value_or(100.0);
                            frame.binning = active_settings_.binning.value_or(1);
                            frame.timestamp_utc = core::Clock::now_utc();
                            frame.clock_sync = core::Clock::get_sync();

                            ++frames_captured_;
                            ++frames_in_second;

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
                    }
                }
            }
            VariantClear(&var_img);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (can_abort_exposure_) {
        com_call_void_method(disp, L"AbortExposure");
    }
    com_put_bool(disp, L"Connected", false);
    pump_sta_messages();
    disp->Release();
    if (should_uninit) CoUninitialize();
}

} // namespace sabit::camera
