#
# Copyright (C) 2010 The Android Open Source Project
# Copyright (C) 2016 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Makefile for producing ScandiumUI SDK coverage reports.
# Run "make scandium-sdk-test-coverage" in the $ANDROID_BUILD_TOP directory.

scandium_sdk_api_coverage_exe := $(HOST_OUT_EXECUTABLES)/scandium-sdk-api-coverage
dexdeps_exe := $(HOST_OUT_EXECUTABLES)/dexdeps

coverage_out := $(HOST_OUT)/scandium-sdk-api-coverage

api_text_description := scandium-sdk/api/scandium_current.txt
api_xml_description := $(coverage_out)/api.xml
$(api_xml_description) : $(api_text_description) $(APICHECK)
	$(hide) echo "Converting API file to XML: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) $(APICHECK_COMMAND) -convert2xml $< $@

scandium-sdk-test-coverage-report := $(coverage_out)/scandium-sdk-test-coverage.html

scandium_sdk_tests_apk := $(call intermediates-dir-for,APPS,ScandiumPlatformTests)/package.apk
scandiumsettingsprovider_tests_apk := $(call intermediates-dir-for,APPS,ScandiumSettingsProviderTests)/package.apk
scandium_sdk_api_coverage_dependencies := $(scandium_sdk_api_coverage_exe) $(dexdeps_exe) $(api_xml_description)

$(scandium-sdk-test-coverage-report): PRIVATE_TEST_CASES := $(scandium_sdk_tests_apk) $(scandiumsettingsprovider_tests_apk)
$(scandium-sdk-test-coverage-report): PRIVATE_SCANDIUM_SDK_API_COVERAGE_EXE := $(scandium_sdk_api_coverage_exe)
$(scandium-sdk-test-coverage-report): PRIVATE_DEXDEPS_EXE := $(dexdeps_exe)
$(scandium-sdk-test-coverage-report): PRIVATE_API_XML_DESC := $(api_xml_description)
$(scandium-sdk-test-coverage-report): $(scandium_sdk_tests_apk) $(scandiumsettingsprovider_tests_apk) $(scandium_sdk_api_coverage_dependencies) | $(ACP)
	$(call generate-scandium-coverage-report,"SCANDIUM-SDK API Coverage Report",\
			$(PRIVATE_TEST_CASES),html)

.PHONY: scandium-sdk-test-coverage
scandium-sdk-test-coverage : $(scandium-sdk-test-coverage-report)

# Put the test coverage report in the dist dir if "scandium-sdk" is among the build goals.
ifneq ($(filter scandium-sdk, $(MAKECMDGOALS)),)
  $(call dist-for-goals, scandium-sdk, $(scandium-sdk-test-coverage-report):scandium-sdk-test-coverage-report.html)
endif

# Arguments;
#  1 - Name of the report printed out on the screen
#  2 - List of apk files that will be scanned to generate the report
#  3 - Format of the report
define generate-scandium-coverage-report
	$(hide) mkdir -p $(dir $@)
	$(hide) $(PRIVATE_SCANDIUM_SDK_API_COVERAGE_EXE) -d $(PRIVATE_DEXDEPS_EXE) -a $(PRIVATE_API_XML_DESC) -f $(3) -o $@ $(2) -cm
	@ echo $(1): file://$@
endef

# Reset temp vars
scandium_sdk_api_coverage_dependencies :=
scandium-sdk-combined-coverage-report :=
scandium-sdk-combined-xml-coverage-report :=
scandium-sdk-verifier-coverage-report :=
scandium-sdk-test-coverage-report :=
api_xml_description :=
api_text_description :=
coverage_out :=
dexdeps_exe :=
scandium_sdk_api_coverage_exe :=
scandium_sdk_verifier_apk :=
android_scandium_sdk_zip :=
