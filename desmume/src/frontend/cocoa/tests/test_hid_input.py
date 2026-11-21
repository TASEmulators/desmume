#!/usr/bin/env python3
"""Cocoa HID parser regression check (macOS with Xcode command-line tools).

Run: python3 desmume/src/frontend/cocoa/tests/test_hid_input.py
Compiles the production parser functions and input structs with controlled IOKit
values. No controller, ROM, or external Python package is required.
"""
from pathlib import Path
import subprocess
import tempfile

COCOA = Path(__file__).resolve().parents[1]
SOURCE = COCOA / "userinterface/InputManager.mm"
HEADER = COCOA / "ClientInputHandler.h"


def block(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


src, header = SOURCE.read_text(), HEADER.read_text()
types = "\n".join(block(header, marker) + ";" for marker in (
    "enum ClientInputDeviceState", "struct ClientInputDeviceProperties\n",
    "struct ClientCommandAttributes\n"))
functions = "\n".join(block(src, marker) for marker in (
    "bool InputElementCodeFromHIDElement(", "bool InputDeviceCodeFromHIDDevice(",
    "ClientInputDeviceProperties InputAttributesOfHIDValue(",
    "ClientInputDevicePropertiesList InputListFromHIDValue("))

prefix = r'''
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDLib.h>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#define INPUT_HANDLER_STRING_LENGTH 256
struct ClientCommandAttributes;
typedef void (*ClientCommandDispatcher)(const ClientCommandAttributes &, void *);
'''
boundary = r'''
typedef std::vector<ClientInputDeviceProperties> ClientInputDevicePropertiesList;
@interface InputManager : NSObject
- (ClientCommandAttributes)mappedCommandAttributesOfDeviceCode:(const char *)device elementCode:(const char *)element;
@end
@implementation InputManager
- (ClientCommandAttributes)mappedCommandAttributesOfDeviceCode:(const char *)device elementCode:(const char *)element { return {}; }
@end
struct Fixture {
    int type = kIOHIDElementTypeInput_Button;
    int page = kHIDPage_Button;
    int usage = 1;
    int min = 0;
    int max = 1;
    int value = 1;
    int length = 1;
    int report = 0;
};
static Fixture f;
static NSDictionary *properties;
#define IOHIDValueGetElement(v) ((IOHIDElementRef)&f)
#define IOHIDElementGetDevice(e) ((IOHIDDeviceRef)&f)
#define IOHIDElementGetLogicalMin(e) (f.min)
#define IOHIDElementGetLogicalMax(e) (f.max)
#define IOHIDElementGetType(e) ((IOHIDElementType)f.type)
#define IOHIDElementGetUsage(e) (f.usage)
#define IOHIDElementGetUsagePage(e) (f.page)
#define IOHIDValueGetIntegerValue(v) (f.value)
#define IOHIDValueGetLength(v) (f.length)
#define IOHIDElementGetReportID(e) (f.report)
#define IOHIDDeviceGetProperty(d,k) ((CFTypeRef)[properties objectForKey:(NSString *)(k)])
bool InputElementNameFromHIDElement(IOHIDElementRef, char *out) { strcpy(out, "Fixture"); return true; }
bool InputDeviceNameFromHIDDevice(IOHIDDeviceRef, char *out) { strcpy(out, "Fixture controller"); return true; }
'''
cases = r'''
int main(int argc, char **argv) { @autoreleasepool {
    properties = @{ @kIOHIDVendorIDKey: @1118, @kIOHIDProductIDKey: @654,
                    @kIOHIDLocationIDKey: @1 };
    std::string test = argv[1];
    InputManager *manager = [InputManager new];
    if (test == "wrong_property_types") {
        properties = @{ @kIOHIDVendorIDKey: @"not a number",
                        @kIOHIDProductIDKey: @"not a number",
                        @kIOHIDLocationIDKey: @"not a number" };
        char code[256] = {};
        assert(InputDeviceCodeFromHIDDevice((IOHIDDeviceRef)&f, code));
        assert(strcmp(code, "0/0/0x00000000") == 0);
    } else if (test == "missing_location") {
        properties = @{ @kIOHIDVendorIDKey: @1118, @kIOHIDProductIDKey: @654 };
        char code[256] = {};
        assert(InputDeviceCodeFromHIDDevice((IOHIDDeviceRef)&f, code));
        assert(strcmp(code, "1118/654/0x00000000") == 0);
    } else if (test == "missing_ids") {
        properties = @{};
        char code[256] = {};
        assert(InputDeviceCodeFromHIDDevice((IOHIDDeviceRef)&f, code));
        assert(strcmp(code, "0/0/0x00000000") == 0);
    } else {
        if (test == "switch_full" || test == "switch_simple") { properties = @{ @kIOHIDVendorIDKey: @0x057e, @kIOHIDProductIDKey: @0x2009, @kIOHIDLocationIDKey: @1 }; f.report = test == "switch_full" ? 0x30 : 0x3f; }
        if (test == "other_full") f.report = 0x30;
        if (test == "invalid_type") f.type = kIOHIDElementTypeOutput;
        if (test == "undefined_page") f.page = 0;
        if (test == "undefined_usage") f.usage = 0;
        if (test == "raw_report") { f.type = kIOHIDElementTypeInput_ScanCodes; f.page = 0; f.usage = -1; f.max = 0; f.value = 0; }
        if (test == "zero_range") { f.type = kIOHIDElementTypeInput_Axis; f.page = kHIDPage_GenericDesktop; f.usage = kHIDUsage_GD_X; f.max = 0; f.value = 0; }
        if (test == "axis") { f.type = kIOHIDElementTypeInput_Axis; f.page = kHIDPage_GenericDesktop; f.usage = kHIDUsage_GD_X; f.max = 65535; f.value = 0; }
        if (test == "hat_up" || test == "hat_neutral") { f.type = kIOHIDElementTypeInput_Misc; f.page = kHIDPage_GenericDesktop; f.usage = kHIDUsage_GD_Hatswitch; f.max = 7; f.value = test == "hat_up" ? 0 : 8; }
        auto result = InputListFromHIDValue((IOHIDValueRef)&f, manager, true);
        if (test == "raw_report" || test == "zero_range" || test == "switch_full" || test == "invalid_type" || test == "undefined_page" || test == "undefined_usage") assert(result.empty());
        if (test == "button" || test == "switch_simple" || test == "other_full") { assert(result.size() == 1); assert(result[0].state == ClientInputDeviceState_On); assert(result[0].scalar == 1.0f); }
        if (test == "axis") { assert(result.size() == 2); assert(result[0].state == ClientInputDeviceState_On); assert(result[1].state == ClientInputDeviceState_Off); }
        if (test == "hat_up" || test == "hat_neutral") {
            assert(result.size() == 4);
            for (size_t i = 0; i < result.size(); ++i) assert(result[i].state == ((i == 0 && test == "hat_up") ? ClientInputDeviceState_On : ClientInputDeviceState_Off));
        }
        for (const auto &value : result) assert(std::isfinite(value.scalar));
    }
    [manager release];
} }
'''
with tempfile.TemporaryDirectory(prefix="desmume-input-test-") as tmp:
    tmp = Path(tmp)
    unit = tmp / "parser.mm"
    unit.write_text(prefix + types + boundary + functions + cases)
    exe = tmp / "parser"
    subprocess.run(["xcrun", "clang++", "-std=c++11", "-Wno-deprecated-declarations",
                    "-Wno-strncat-size", str(unit), "-framework", "Foundation",
                    "-framework", "IOKit", "-o", str(exe)], check=True)
    failed = []
    for name in ("raw_report", "invalid_type", "undefined_page", "undefined_usage",
                 "zero_range", "missing_location", "missing_ids",
                 "wrong_property_types", "button", "axis", "hat_up", "hat_neutral", "switch_full", "switch_simple", "other_full"):
        result = subprocess.run([str(exe), name], capture_output=True)
        print(f"{'PASS' if result.returncode == 0 else 'FAIL'} {name}", flush=True)
        if result.returncode:
            failed.append(name)
            print(result.stderr.decode(errors="replace").strip()[:300])
    raise SystemExit(bool(failed))
