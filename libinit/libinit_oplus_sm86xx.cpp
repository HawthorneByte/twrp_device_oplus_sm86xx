/*
 * Copyright (C) 2022-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <fs_mgr.h>
#include <unordered_map>

using android::base::GetProperty;
using android::base::ParseInt;
using android::fs_mgr::GetKernelCmdline;

const std::unordered_map<int, std::string> kRegionSuffixMap = {
    {27,    "IN"},
    {55,    "RU"},
    {68,    "EEA"},
    {151,   ""},    // CN
    {161,   "NA"},
    {167,   ""},    // GLO
    {0,     ""},    // Default
};

struct ModelInfo {
    const char* brand;              // ro.product.brand
    const char* device;             // ro.product.device
    const char* manufacturer;       // ro.product.manufacturer
    const char* model;              // ro.product.model
    const char* base_name;          // ro.product.name  w/o region suffix
    const char* twversion;          // ro.twrp.device_version
    const char* supportSpr;         // vendor.display.enable_spr
    const char* y_offset;           // ro.twrp.y_offset
    const char* h_offset;           // ro.twrp.h_offset
};

const std::unordered_map<int, ModelInfo> kModelInfoMap = {
    //                                                                              brand     device       manufacturer model      base_name  twversion                   spr y_off h_off
    {22825, {"OnePlus", "OP5929L1", "OnePlus", "PJD110",    "PJD110",  "OnePlus_12",                   "0", "121",  "-121"}},
    {23607, {"Realme",  "RE5C37",   "Realme",  "RMX3888",   "RMX3888", "Realme_GT_5_Pro",              "0", "108",  "-108"}},
    {23631, {"Realme",  "RE5C4FL1", "Realme",  "RMX3800",   "RMX3800", "Realme_GT_6",                  "0", "108",  "-108"}},
    {23803, {"OnePlus", "OP5CFBL1", "OnePlus", "PJF110",    "PJF110",  "OnePlus_ACE_3_V",              "0", "108",  "-108"}},
    {23814, {"OnePlus", "OP5D06L1", "OnePlus", "PJX110",    "PJX110",  "OnePlus_ACE_3_Pro",            "0", "112",  "-112"}},
    {23851, {"OnePlus", "OP5D2BL1", "OnePlus", "PKG110",    "PKG110",  "OnePlus_ACE_5",                "0", "112",  "-112"}},
    {0,     {"OPLUS",   "SM86XX",   "OPLUS",   "SM86XX",    "SM86XX",  "OPLUS",                        "0", "121",  "-121"}}, // Default
};

/*
 * SetProperty does not allow updating read only properties and as a result
 * does not work for our use case. Write "OverrideProperty" to do practically
 * the same thing as "SetProperty" without this restriction.
 */
void OverrideProperty(const char* name, const char* value) {
    size_t valuelen = strlen(value);

    prop_info* pi = (prop_info*)__system_property_find(name);
    if (pi != nullptr) {
        __system_property_update(pi, value, valuelen);
    } else {
        __system_property_add(name, strlen(name), value, valuelen);
    }
}

void SetupModelProperties(const ModelInfo& info, const std::string& region) {
    struct PropPair {
        const char* key;
        const char* value;
    } props[] = {
        {"ro.product.brand",                info.brand},
        {"ro.product.device",               info.device},
        {"ro.product.manufacturer",         info.manufacturer},
        {"ro.product.model",                info.model},
        {"vendor.display.enable_spr",       info.supportSpr},
        {"ro.twrp.device_version",          info.twversion},
        {"ro.twrp.y_offset",                info.y_offset},
        {"ro.twrp.h_offset",                info.h_offset},
        {"ro.build.date.utc",               "0"},
    };
    for (const auto& p : props) {
        OverrideProperty(p.key, p.value);
    }

    // Set ro.product.name separately to avoid lifetime issues with c_str()
    std::string full_name = info.base_name + region;
    OverrideProperty("ro.product.name", full_name.c_str());
}

void vendor_load_properties() {
    std::string buf = "0";
    GetKernelCmdline("oplus_region", &buf);

    int region = 0;
    ParseInt(buf, &region);
    auto region_suffix_iter = kRegionSuffixMap.find(region);

    // Handle unknown regions gracefully
    if (region_suffix_iter == kRegionSuffixMap.end()) {
        LOG(WARNING) << "Unknown oplus_region: " << region << ", using default";
        region_suffix_iter = kRegionSuffixMap.find(0);
    }

    int prjname = 0;
    ParseInt(GetProperty("ro.boot.prjname", "0"), &prjname);
    auto model_info = kModelInfoMap.find(prjname);

    // Handle unknown device models
    if (model_info == kModelInfoMap.end()) {
        LOG(ERROR) << "Unknown prjname: " << prjname << ", using default";
        model_info = kModelInfoMap.find(0);
    }

    SetupModelProperties(model_info->second, region_suffix_iter->second);
}