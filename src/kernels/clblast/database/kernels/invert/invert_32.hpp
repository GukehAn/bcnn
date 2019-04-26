
// =================================================================================================
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. It
// is auto-generated by the 'scripts/database/database.py' Python script.
//
// This file populates the database with best-found tuning parameters for the 'Invert32' kernels.
//
// =================================================================================================

namespace clblast {
namespace database {

const DatabaseEntry InvertSingle = {
  "Invert", Precision::kSingle, {"INTERNAL_BLOCK_SIZE", "LOCALPAD", "TMMWGSX", "TMMWGSY"}, {
    { // Intel GPUs
      kDeviceTypeGPU, "Intel", {
        { "default", {
          { Name{"Intel(R) HD Graphics Skylake ULT GT2              "}, Params{ 16, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
          { kDeviceNameDefault                                        , Params{ 16, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        } },
      }
    },
    { // Default
      kDeviceTypeAll, "default", {
        { "default", {
          { kDeviceNameDefault                                        , Params{ 16, 0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
        } },
      }
    },
  }
};

} // namespace database
} // namespace clblast
