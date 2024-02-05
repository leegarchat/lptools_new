# Project Compilation Instructions

To compile the binary file from this code, you need to install Linux or WSL on Windows.

1. Download the Android source code (requires 200 to 300 gigabytes of disk space):

    ```bash
    cd ~
    mkdir aosp
    cd aosp
    repo init -u https://android.googlesource.com/platform/manifest -b <Android_version>
    ```

    For example:

    ```bash
    repo init -u https://android.googlesource.com/platform/manifest -b android-14.0.0_r9
    ```

    To view available Android versions, use:

    ```bash
    git ls-remote --tags https://android.googlesource.com/platform/manifest | grep 'refs/tags/android-[0-9]' | awk '{print $2}'
    ```

2. Synchronize the repository:

    ```bash
    repo sync -j$(nproc)
    ```

3. After synchronization, place `lptools.cc` and `Android.bp` in the `aosp/external/lptools/` folder.

4. Make changes to `aosp/external/boringssl/Android.bp` before compilation:

    Add to `cc_library_static` in `visibility`:

    ```bash
    "//external/lptools",
    ```
    ```cpp
    // Static library
    // This version of libcrypto will not have FIPS self tests enabled, so its
    // usage is protected through visibility to ensure it doesn't end up used
    // somewhere that needs the FIPS version.
    cc_library_static {
        name: "libcrypto_static",
        visibility: [
            "//art/build/sdk",
            "//bootable/recovery/updater",
            "//external/conscrypt",
            "//external/python/cpython2",
            "//external/rust/crates/quiche",
            // Strictly, only the *static* toybox for legacy devices should have
            // access to libcrypto_static, but we can't express that.
            "//external/toybox",
            "//hardware/interfaces/confirmationui/1.0/vts/functional",
            "//hardware/interfaces/drm/1.0/vts/functional",
            "//hardware/interfaces/drm/1.2/vts/functional",
            "//hardware/interfaces/drm/1.3/vts/functional",
            "//hardware/interfaces/keymaster/3.0/vts/functional",
            "//hardware/interfaces/keymaster/4.0/vts/functional",
            "//hardware/interfaces/keymaster/4.1/vts/functional",
            "//packages/modules/adb",
            "//packages/modules/DnsResolver/tests:__subpackages__",
            "//packages/modules/NeuralNetworks:__subpackages__",
            "//system/core/init",
            "//system/core/fs_mgr/liblp",
            "//system/core/fs_mgr/liblp/vts_core",
            "//system/core/fs_mgr/libsnapshot",
            "//system/libvintf/test",
            "//system/security/keystore/tests",
            "//test/vts-testcase/security/avb",
            "//external/lptools",  // <<<----- Add your module to visibility
        ],
        apex_available: [
            "//apex_available:platform",
            "com.android.neuralnetworks",
        ],
        defaults: [
            "libcrypto_bcm_sources",
            "libcrypto_sources",
            "libcrypto_defaults",
            "boringssl_defaults",
            "boringssl_flags",
        ],
    }

5. Load functions from `envsetup.sh`:

    ```bash
    source build/envsetup.sh
    ```

6. Execute the `lunch` command to select the architecture. You can choose one of the following:

    ```bash
    lunch aosp_arm-eng
    lunch aosp_arm64-eng
    lunch aosp_x86-eng
    lunch aosp_x86_64-eng
    ```

    For example:

    ```bash
    lunch aosp_arm64-eng
    ```

7. Compile using the following command:

    ```bash
    mmma external/lptools
    ```

    The output file will be located in:

    ```
    aosp/out/target/product/generic_{Architecture_Name}/obj/EXECUTABLES/lptools_new_static_intermediates/lptools_new_static
    ```

8. Batch compilation for all 4 architectures can be done with the provided Bash script:

    ```bash
    #!/bin/bash
    pathfile=$(realpath "$0")
    pathfile=$(dirname $pathfile)
    source $pathfile/build/envsetup.sh
    cd $pathfile
    for arch in aosp_arm-eng aosp_arm64-eng aosp_x86-eng aosp_x86_64-eng ; do
        if [ "$arch" == "aosp_arm-eng" ] ; then
            dir_arch=${arch/"arm-eng"/}
            dir_arch=${dir_arch/"aosp_"/"generic"}
        else 
            dir_arch=${arch/"-eng"/}
            dir_arch=${dir_arch/"aosp_"/"generic_"}
        fi
        name_arch=${arch/"-eng"/}
        name_arch=${name_arch/"aosp_"/}
        lunch "$arch"
        mmma external/lptools/ && {
            [ -d /mnt/c/lptools_new_binary ] || mkdir /mnt/c/lptools_new_binary
            cp $pathfile/out/target/product/${dir_arch}/obj/EXECUTABLES/lptools_new_static_intermediates/lptools_new_static /mnt/c/lptools_new_binary/lptools_new_$name_arch
        }
    done
    ```

   This script compiles and copies the binaries to the specified directory (`/mnt/c/lptools_new_binary` in this example).

## lptools_new_static - Binary Functionality

The `lptools_new_static` binary provides various functionalities for managing Android Logical Partitions. Below is a description of the available options:

### Basic Configuration:

- `--suffix <_a|a|0|_b|b|1>`
  - By default, it will be taken from `ro.boot.slot_suffix`.

- `--slot <0|1>`
  - Slot 0 will be used by default.

- `--super </path/to/super>`
  - By default, the standard path `/dev/block/by-name/super` will be used.

- `--group <group_name_in_super>`
  - By default, the relative section of `system + --suffix` in the specified `--slot` will be searched.

### Available Options:

- `--create <partition name> <partition size>`
- `--remove <partition name>`
- `--resize <partition name> <new size>`
- `--replace <original partition name> <new partition name>`
- `--map <partition name>`
- `--unmap <partition name>`
- `--free`
- `--unlimited-group`
- `--clear-cow`
- `--get-info`

### Usage:

```bash
./lptools_new_static <options>
```
**Note:** Ensure you have the necessary privileges to perform the specified operations.

**Caution:** Please be aware that incorrect usage of the `lptools_new_static` binary may lead to data loss or system instability. Exercise caution and ensure proper understanding of the provided options before proceeding.

