// Copyright (c) 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests for OpExtension validator rules.

#include <string>

#include "enum_string_mapping.h"
#include "extensions.h"
#include "gmock/gmock.h"
#include "test_fixture.h"
#include "unit_spirv.h"
#include "val_fixtures.h"

namespace {

using ::libspirv::Extension;

using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Values;
using ::testing::ValuesIn;

using std::string;

using ValidateKnownExtensions = spvtest::ValidateBase<string>;
using ValidateUnknownExtensions = spvtest::ValidateBase<string>;
using ValidateExtensionCapabilities = spvtest::ValidateBase<bool>;

// Returns expected error string if |extension| is not recognized.
string GetErrorString(const std::string& extension) {
  return "Found unrecognized extension " + extension;
}

INSTANTIATE_TEST_CASE_P(
    ExpectSuccess, ValidateKnownExtensions,
    Values(
        // Match the order as published on the SPIR-V Registry.
        "SPV_AMD_shader_explicit_vertex_parameter",
        "SPV_AMD_shader_trinary_minmax", "SPV_AMD_gcn_shader",
        "SPV_KHR_shader_ballot", "SPV_AMD_shader_ballot",
        "SPV_AMD_gpu_shader_half_float", "SPV_KHR_shader_draw_parameters",
        "SPV_KHR_subgroup_vote", "SPV_KHR_16bit_storage",
        "SPV_KHR_device_group", "SPV_KHR_multiview",
        "SPV_NVX_multiview_per_view_attributes", "SPV_NV_viewport_array2",
        "SPV_NV_stereo_view_rendering", "SPV_NV_sample_mask_override_coverage",
        "SPV_NV_geometry_shader_passthrough", "SPV_AMD_texture_gather_bias_lod",
        "SPV_KHR_storage_buffer_storage_class", "SPV_KHR_variable_pointers",
        "SPV_AMD_gpu_shader_int16", "SPV_KHR_post_depth_coverage",
        "SPV_KHR_shader_atomic_counter_ops", "SPV_EXT_shader_stencil_export",
        "SPV_EXT_shader_viewport_index_layer",
        "SPV_AMD_shader_image_load_store_lod", "SPV_AMD_shader_fragment_mask"));

INSTANTIATE_TEST_CASE_P(FailSilently, ValidateUnknownExtensions,
                        Values("ERROR_unknown_extension", "SPV_KHR_",
                               "SPV_KHR_shader_ballot_ERROR"));

TEST_P(ValidateKnownExtensions, ExpectSuccess) {
  const std::string extension = GetParam();
  const string str =
      "OpCapability Shader\nOpCapability Linkage\nOpExtension \"" + extension +
      "\"\nOpMemoryModel Logical GLSL450";
  CompileSuccessfully(str.c_str());
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), Not(HasSubstr(GetErrorString(extension))));
}

TEST_P(ValidateUnknownExtensions, FailSilently) {
  const std::string extension = GetParam();
  const string str =
      "OpCapability Shader\nOpCapability Linkage\nOpExtension \"" + extension +
      "\"\nOpMemoryModel Logical GLSL450";
  CompileSuccessfully(str.c_str());
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), HasSubstr(GetErrorString(extension)));
}

TEST_F(ValidateExtensionCapabilities, DeclCapabilitySuccess) {
  const string str =
      "OpCapability Shader\nOpCapability Linkage\nOpCapability DeviceGroup\n"
      "OpExtension \"SPV_KHR_device_group\""
      "\nOpMemoryModel Logical GLSL450";
  CompileSuccessfully(str.c_str());
  ASSERT_EQ(SPV_SUCCESS, ValidateInstructions());
}

TEST_F(ValidateExtensionCapabilities, DeclCapabilityFailure) {
  const string str =
      "OpCapability Shader\nOpCapability Linkage\nOpCapability DeviceGroup\n"
      "\nOpMemoryModel Logical GLSL450";
  CompileSuccessfully(str.c_str());
  ASSERT_EQ(SPV_ERROR_MISSING_EXTENSION, ValidateInstructions());
  EXPECT_THAT(getDiagnosticString(), HasSubstr("1st operand of Capability"));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr("requires one of these extensions"));
  EXPECT_THAT(getDiagnosticString(), HasSubstr("SPV_KHR_device_group"));
}


using ValidateAMDShaderBallotCapabilities = spvtest::ValidateBase<string>;

// Returns a vector of strings for the prefix of a SPIR-V assembly shader
// that can use the group instructions introduced by SPV_AMD_shader_ballot.
std::vector<string> ShaderPartsForAMDShaderBallot() {
  return std::vector<string>{R"(
  OpCapability Shader
  OpCapability Linkage
  )",
                             R"(
  OpMemoryModel Logical GLSL450
  %float = OpTypeFloat 32
  %uint = OpTypeInt 32 0
  %int = OpTypeInt 32 1
  %scope = OpConstant %uint 3
  %uint_const = OpConstant %uint 42
  %int_const = OpConstant %uint 45
  %float_const = OpConstant %float 3.5

  %void = OpTypeVoid
  %fn_ty = OpTypeFunction %void
  %fn = OpFunction %void None %fn_ty
  %entry = OpLabel
  )"};
}

// Returns a list of SPIR-V assembly strings, where each uses only types
// and IDs that can fit with a shader made from parts from the result
// of ShaderPartsForAMDShaderBallot.
std::vector<string> AMDShaderBallotGroupInstructions() {
  return std::vector<string>{
  "%iadd_reduce = OpGroupIAddNonUniformAMD %uint %scope Reduce %uint_const",
  "%iadd_iscan = OpGroupIAddNonUniformAMD %uint %scope InclusiveScan %uint_const",
  "%iadd_escan = OpGroupIAddNonUniformAMD %uint %scope ExclusiveScan %uint_const",

  "%fadd_reduce = OpGroupFAddNonUniformAMD %float %scope Reduce %float_const",
  "%fadd_iscan = OpGroupFAddNonUniformAMD %float %scope InclusiveScan %float_const",
  "%fadd_escan = OpGroupFAddNonUniformAMD %float %scope ExclusiveScan %float_const",

  "%fmin_reduce = OpGroupFMinNonUniformAMD %float %scope Reduce %float_const",
  "%fmin_iscan = OpGroupFMinNonUniformAMD %float %scope InclusiveScan %float_const",
  "%fmin_escan = OpGroupFMinNonUniformAMD %float %scope ExclusiveScan %float_const",

  "%umin_reduce = OpGroupUMinNonUniformAMD %uint %scope Reduce %uint_const",
  "%umin_iscan = OpGroupUMinNonUniformAMD %uint %scope InclusiveScan %uint_const",
  "%umin_escan = OpGroupUMinNonUniformAMD %uint %scope ExclusiveScan %uint_const",

  "%smin_reduce = OpGroupUMinNonUniformAMD %int %scope Reduce %int_const",
  "%smin_iscan = OpGroupUMinNonUniformAMD %int %scope InclusiveScan %int_const",
  "%smin_escan = OpGroupUMinNonUniformAMD %int %scope ExclusiveScan %int_const",

  "%fmax_reduce = OpGroupFMaxNonUniformAMD %float %scope Reduce %float_const",
  "%fmax_iscan = OpGroupFMaxNonUniformAMD %float %scope InclusiveScan %float_const",
  "%fmax_escan = OpGroupFMaxNonUniformAMD %float %scope ExclusiveScan %float_const",

  "%umax_reduce = OpGroupUMaxNonUniformAMD %uint %scope Reduce %uint_const",
  "%umax_iscan = OpGroupUMaxNonUniformAMD %uint %scope InclusiveScan %uint_const",
  "%umax_escan = OpGroupUMaxNonUniformAMD %uint %scope ExclusiveScan %uint_const",

  "%smax_reduce = OpGroupUMaxNonUniformAMD %int %scope Reduce %int_const",
  "%smax_iscan = OpGroupUMaxNonUniformAMD %int %scope InclusiveScan %int_const",
  "%smax_escan = OpGroupUMaxNonUniformAMD %int %scope ExclusiveScan %int_const"
  };
}

TEST_P(ValidateAMDShaderBallotCapabilities, ExpectSuccess) {
  // Succeed because the module specifies the SPV_AMD_shader_ballot extension.
  auto parts = ShaderPartsForAMDShaderBallot();

  const string assembly = parts[0] + "OpExtension \"SPV_AMD_shader_ballot\"\n" +
                          parts[1] + GetParam() + "\nOpReturn OpFunctionEnd";

  CompileSuccessfully(assembly.c_str());
  EXPECT_EQ(SPV_SUCCESS, ValidateInstructions()) << getDiagnosticString();
}

INSTANTIATE_TEST_CASE_P(ExpectSuccess, ValidateAMDShaderBallotCapabilities,
                        ValuesIn(AMDShaderBallotGroupInstructions()));

TEST_P(ValidateAMDShaderBallotCapabilities, ExpectFailure) {
  // Fail because the module does not specify the SPV_AMD_shader_ballot extension.
  auto parts = ShaderPartsForAMDShaderBallot();

  const string assembly =
      parts[0] + parts[1] + GetParam() + "\nOpReturn OpFunctionEnd";

  CompileSuccessfully(assembly.c_str());
  EXPECT_EQ(SPV_ERROR_INVALID_CAPABILITY, ValidateInstructions());

  // Make sure we get an appropriate error message.
  // Find just the opcode name, skipping over the "Op" part.
  auto prefix_with_opcode = GetParam().substr(GetParam().find("Group"));
  auto opcode = prefix_with_opcode.substr(0, prefix_with_opcode.find(' '));
  EXPECT_THAT(getDiagnosticString(),
              HasSubstr(string("Opcode " + opcode +
                               " requires one of these capabilities: Groups")));
}

INSTANTIATE_TEST_CASE_P(ExpectFailure, ValidateAMDShaderBallotCapabilities,
                        ValuesIn(AMDShaderBallotGroupInstructions()));

}  // anonymous namespace
