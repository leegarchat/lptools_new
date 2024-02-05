#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <cmath>

#include <optional>
#include <regex>
#include <chrono>

#include <getopt.h>
#include <inttypes.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <cutils/android_get_control_file.h>
#include <fs_mgr.h>
#include <liblp/builder.h>
#include <liblp/liblp.h>
#include <fs_mgr_dm_linear.h>
#include <libdm/dm.h>
#ifndef LPTOOLS_STATIC
#include <android/hardware/boot/1.1/IBootControl.h>
#include <android/hardware/boot/1.1/types.h>
#endif

using namespace android;
using namespace android::fs_mgr;
using namespace std;
using namespace std::literals;


bool UpdateAllPartitionMetadata(const string& super_name,
                                const android::fs_mgr::LpMetadata& metadata,
                                const uint32_t user_slot) {
    bool ok = true;
    ok &= UpdatePartitionTable(super_name, metadata, user_slot);
    return ok;
}

class PartitionBuilder {
public:
    explicit PartitionBuilder(string user_suffix, bool user_slot, string user_super);
    
    bool Write();
    bool Valid() const { return !!builder_; }
    MetadataBuilder* operator->() const { return builder_.get(); }

private:
    string super_device_;
    string slot_suffix_;
    uint32_t slot_number_;
    std::unique_ptr<MetadataBuilder> builder_;
};

PartitionBuilder::PartitionBuilder(string user_suffix, bool user_slot, string user_super) {
    slot_number_ = user_slot;
    slot_suffix_ = user_suffix;
    auto super_device = std::move(user_super);
    super_device_ = std::move(super_device);
    builder_ = MetadataBuilder::New(super_device_, slot_number_);
}
bool saveToDisk(PartitionBuilder builder) {
    return builder.Write();
}
bool PartitionBuilder::Write() {
    auto metadata = builder_->Export();
    if (!metadata) {
        return false;
    }
    return UpdateAllPartitionMetadata(super_device_, *metadata.get(), slot_number_);
}


bool fileOrBlockDeviceExists(const string& path) {
    std::ifstream file(path);
    return file.good();
}
bool isDirectory(const string& path) {
    struct stat path_stat;
    stat(path.c_str(), &path_stat);
    return S_ISDIR(path_stat.st_mode);
}
inline bool ends_with(string const & value, string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void Help_menu() {
    std::cout << "Basic configuration:\n\n";
    std::cout << "  --suffix <_a|a|0|_b|b|1>\n";
    std::cout << "      By default, it will be taken from ro.boot.slot_suffix\n\n";
    std::cout << "  --slot <0|1>\n";
    std::cout << "      Slot 0 will be used by default\n";
    std::cout << "  --super </path/to/super>\n";
    std::cout << "      By default, the standard path /dev/block/by-name/super will be used\n\n";
    std::cout << "  --group <group_name_in_super>\n";
    std::cout << "      By default, the relative section of system + --suffix in the specified --slot will be searched\n\n";
    std::cout << "Please use one of the following options:\n";
    std::cout << "  --create <partition name> <partition size>\n";
    std::cout << "  --remove <partition name>\n";
    std::cout << "  --resize <partition name> <newsize>\n";
    std::cout << "  --replace <original partition name> <new partition name>\n";
    std::cout << "  --map <partition name>\n";
    std::cout << "  --unmap <partition name>\n";
    std::cout << "  --free\n";
    std::cout << "  --unlimited-group\n";
    std::cout << "  --clear-cow\n";
    std::cout << "  --get-info\n\n";
    exit(1);
}

int main(int argc, char* argv[]) {
    std::vector<string> arguments(argv + 1, argv + argc);

    int slotValue = 0;
    std::string suffixValue = ::android::base::GetProperty("ro.boot.slot_suffix", "");
    std::string superPath = "/dev/block/by-name/super";
    std::string groupValue;
    
    for (size_t i = 0; i < arguments.size();) {
        if (arguments[i] == "--slot") {
            if (i + 1 < arguments.size()) {
                string slotCandidate = arguments[i + 1];
                if (slotCandidate == "0" || slotCandidate == "1") {
                    slotValue = std::stoi(slotCandidate);
                    arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
                } else {
                    std::cerr << "Error: Invalid value for --slot. Should be '0' or '1'." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: --slot requires a value. Should be '0' or '1'." << std::endl;
                return 1;
            }
        } else if (arguments[i] == "--suffix") {
            if (i + 1 < arguments.size()) {
                string suffixCandidate = arguments[i + 1];
                if (suffixCandidate == "_a" || suffixCandidate == "a" || suffixCandidate == "0") {
                    suffixValue = "_a";
                } else if (suffixCandidate == "_b" || suffixCandidate == "b" || suffixCandidate == "1") {
                    suffixValue = "_b";
                } else {
                    std::cerr << "Error: Invalid value for --suffix. Should be '_a', 'a', '0', '_b', 'b', or '1'." << std::endl;
                    return 1;
                }
                arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            } else {
                std::cerr << "Error: --suffix requires a value." << std::endl;
                return 1;
            }
        } else if (arguments[i] == "--group") {
            if (i + 1 < arguments.size()) {
                groupValue = arguments[i + 1];
                // Проверка, что строка для --group не начинается с --
                if (groupValue.compare(0, 2, "--") == 0) {
                    std::cerr << "Error: The value for --group should not start with '--'." << std::endl;
                    return 1;
                }

                
                arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            } else {
                std::cerr << "Error: --group requires a value." << std::endl;
                return 1;
            }
        } else if (arguments[i] == "--super") {
            if (i + 1 < arguments.size()) {
                superPath = arguments[i + 1];
                if (!fileOrBlockDeviceExists(superPath)) {
                    std::cerr << "Error: The specified path for --super does not exist: " << superPath << std::endl;
                    return 1;
                } else if (isDirectory(superPath)) {
                    std::cerr << "Error: The specified path for --super is a directory: " << superPath << std::endl;
                    return 1;
                }
                arguments.erase(arguments.begin() + i, arguments.begin() + i + 2);
            } else {
                std::cerr << "Error: --super requires a value." << std::endl;
                return 1;
            }
        } else {
            ++i;
        }
    }
    if (!fileOrBlockDeviceExists(superPath)) {
        std::cerr << "Super section along the standard path" << superPath << " not found, indicate the independent path -SUPER </path/to/super>" << std::endl;
        return 1;
    }
    if (arguments.size() < 1) {
        Help_menu();
        exit(1);
    }
    PartitionBuilder builder(suffixValue, slotValue, superPath);
    if (groupValue.empty()) {
        auto groups = builder->ListGroups();
        auto partitionName = "system" + suffixValue;
        for (auto groupName : groups) {
            auto partitions = builder->ListPartitionsInGroup(groupName);
            for (const auto& partition : partitions) {
                if (partition->name() == partitionName) {
                    groupValue = groupName;
                    goto outerLoopEnd;
                }
            }
        }
    } else {
        auto groups = builder->ListGroups();
        
        // Проверка существования groupValue в списке groups
        if (std::find(groups.begin(), groups.end(), groupValue) == groups.end()) {
            std::cerr << "Error: Specified group '" << groupValue << "' does not exist." << std::endl;
            return 1;
        }
    }

    outerLoopEnd:

    std::cout << "Slot: " << slotValue << std::endl;
    std::cout << "Suffix: " << suffixValue << std::endl;
    std::cout << "Path to super: " << superPath << std::endl;
    std::cout << "Group: " << groupValue << std::endl;
    std::cout << "Arguments:";

    for (const auto& arg : arguments) {
        std::cout << " " << arg;
    }
    std::cout << std::endl;

    if (arguments[0] == "--create" ) {
        if (arguments.size() != 3) {
            std::cout << "--create <partition name> <partition size>" << std::endl;
            return 1;
        }
        char* end;
        auto partitionSize = strtoll(arguments[2].c_str(), &end, 0);

        // Проверяем, были ли ошибки при преобразовании.
        if (errno == ERANGE || *end != '\0') {
            std::cerr << "Error: Invalid or out-of-range number." << std::endl;
            return 1;
        }

        // Проверяем, что это число больше или равно нулю.
        if (partitionSize >= 0) {

            
            std::cout << "Create functions " << partitionSize << std::endl;
            string partName = arguments[1]; 
            cout << groupValue << endl;
            auto partition = builder->FindPartition(partName);
            if(partition != nullptr) {
                std::cerr << "Partition " << partName << " already exists." << std::endl;
                exit(1);
            }

            partition = builder->AddPartition(partName, groupValue, 0);
            cout << partition << endl;
            if(partition == nullptr) {
                std::cerr << "Failed to add partition" << std::endl;
                exit(1);
            }
            auto result = builder->ResizePartition(partition, partitionSize);
            std::cout << "Growing partition " << result << std::endl;
            if(!result) {
                std::cerr << "Not enough space to resize partition" << std::endl;
                exit(1);
            }
            if(!saveToDisk(std::move(builder))) {
                std::cerr << "Failed to write partition table" << std::endl;
                exit(1);
            }
            string dmPath;
            CreateLogicalPartitionParams params {
                    .block_device = superPath,
                    .metadata_slot = slotValue,
                    .partition_name = partName,
                    .timeout_ms = std::chrono::milliseconds(10000),
                    .force_writable = true,
            };
            auto dmCreateRes = android::fs_mgr::CreateLogicalPartition(params, &dmPath);
            if(!dmCreateRes) {
                std::cerr << "Could not map partition: " << partName << std::endl;
                exit(1);
            }    
            std::cout << "Creating dm partition for " << partName << " answered " << dmCreateRes << " at " << dmPath << std::endl;
            return 0;
        } else {
            std::cout << "The size of the section should be larger or equal to zero." << std::endl;
            exit(1);
        }
    } else if (arguments[0] == "--remove" ) {
        if (arguments.size() != 2) {
            std::cout << "--remove <partition name>" << std::endl;
            exit(1);
        }
        string partName = arguments[1];
        auto dmState = android::dm::DeviceMapper::Instance().GetState(partName);
        if(dmState == android::dm::DmDeviceState::ACTIVE) {
            android::fs_mgr::DestroyLogicalPartition(partName);
        }
        builder->RemovePartition(partName);
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        cout << "Successful removal of the section: " << partName << endl;
        exit(0);
    } else if (arguments[0] == "--resize" ) {
        if (arguments.size() != 3) {
            std::cout << "--resize <partition name> <newsize>" << std::endl;
            exit(1);
        }
        string partName = arguments[1];
        char* end;
        auto partitionSize = strtoll(arguments[2].c_str(), &end, 0);

        // Проверяем, были ли ошибки при преобразовании.
        if (errno == ERANGE || *end != '\0') {
            std::cerr << "Error: Invalid or out-of-range number." << std::endl;
            return 1;
        }
        if (partitionSize >= 0) {
            bool is_map_need = false;
            auto partition = builder->FindPartition(partName);
            if(partition == nullptr) {
                std::cerr << "Partition does not exist" << std::endl;
                exit(1);
            }
            auto dmState = android::dm::DeviceMapper::Instance().GetState(partName);
            if(dmState == android::dm::DmDeviceState::ACTIVE) {
                if (android::fs_mgr::DestroyLogicalPartition(partName)) {
                    is_map_need = true;
                }
                
            }
            auto result = builder->ResizePartition(partition, partitionSize);
            if(!result) {
                std::cerr << "Not enough space to resize partition" << std::endl;
                exit(1);
            }
            if(!saveToDisk(std::move(builder))) {
                std::cerr << "Failed to write partition table" << std::endl;
                exit(1);
            }
            std::cout << "Resizing partition " << result << std::endl;
            if (is_map_need) {
                string dmPath;
                CreateLogicalPartitionParams params {
                        .block_device = superPath,
                        .metadata_slot = slotValue,
                        .partition_name = partName,
                        .timeout_ms = std::chrono::milliseconds(10000),
                        .force_writable = true,
                };
                auto dmCreateRes = android::fs_mgr::CreateLogicalPartition(params, &dmPath);
                if(!dmCreateRes) {
                    std::cerr << "Could not map partition: " << partName << std::endl;
                    exit(1);
                }
                std::cout << "Creating dm partition for " << partName << " answered " << dmCreateRes << " at " << dmPath << std::endl;
            }
            cout << "Successful change in section for the section:" << partName << endl;
            exit(0);
        } else {
            std::cout << "The size of the section should be larger or equal to zero." << std::endl;
            exit(1);
        }
    } else if (arguments[0] == "--replace" ) {
        if (arguments.size() != 3) {
            std::cout << "--replace <original partition name> <new partition name>" << std::endl;
            exit(1);
        }
        auto OriginalPartName = arguments[1];
        auto NewPartName = arguments[2];
        auto OriginalPartition = builder->FindPartition(OriginalPartName);
        if(OriginalPartition == nullptr) {
            std::cerr << "The original section was not found, maybe you meant " << OriginalPartName << suffixValue << "?" << std::endl;
            exit(1);
        }
        auto NewPartition = builder->FindPartition(NewPartName);
        std::vector<std::unique_ptr<Extent>> originalExtents;
        const auto& extents = OriginalPartition->extents();
        for(unsigned i=0; i<extents.size(); i++) {
            const auto& extend = extents[i];
            auto linear = extend->AsLinearExtent();
            if(linear != nullptr) {
                auto copyLinear = std::make_unique<LinearExtent>(linear->num_sectors(), linear->device_index(), linear->physical_sector());
                originalExtents.push_back(std::move(copyLinear));
            } else {
                auto copyZero = std::make_unique<ZeroExtent>(extend->num_sectors());
                originalExtents.push_back(std::move(copyZero));
            }
            builder->RemovePartition(OriginalPartName);
            builder->RemovePartition(NewPartName);
            auto NewPartition = builder->AddPartition(NewPartName, groupValue, 0);
            if(NewPartition == nullptr) {
                std::cerr << "Failed to add partition" << std::endl;
                exit(1);
            }
            for(auto&& extent: originalExtents) {
                NewPartition->AddExtent(std::move(extent));
            }
            if(!saveToDisk(std::move(builder))) {
                std::cerr << "Failed to write partition table" << std::endl;
                exit(1);
            }
        }

    } else if (arguments[0] == "--map" ) {
        if (arguments.size() != 2) {
            std::cout << "--map <partition name>" << std::endl;
            exit(1);
        }
        string partName = arguments[1];
        string dmPath;
        CreateLogicalPartitionParams params {
                .block_device = superPath,
                .metadata_slot = slotValue,
                .partition_name = partName,
                .timeout_ms = std::chrono::milliseconds(10000),
                .force_writable = true,
        };
        auto dmCreateRes = android::fs_mgr::CreateLogicalPartition(params, &dmPath);
        if(!dmCreateRes) {
            std::cerr << "Could not map partition: " << partName << std::endl;
            exit(1);
        }
        std::cout << "Creating dm partition for " << partName << " answered " << dmCreateRes << " at " << dmPath << std::endl;
        exit(0);
    } else if (arguments[0] == "--unmap" ) {
        if (arguments.size() != 2) {
            std::cout << "--unmap <partition name>" << std::endl;
            exit(1);
        }
        string partName = arguments[1];
        auto dmState = android::dm::DeviceMapper::Instance().GetState(partName);
        if(dmState == android::dm::DmDeviceState::ACTIVE) {
            if (!android::fs_mgr::DestroyLogicalPartition(partName)) {
                std::cerr << "Unable to unmap " << partName << std::endl;
                exit(1);
            }
        }
        cout << "Successful remote Mapping DM section: " << partName << endl;
        exit(0);
    } else if (arguments[0] == "--free" ) {
        if (arguments.size() != 1) {
            std::cout << "--free" << std::endl;
            exit(1);
        }

        auto group_number = builder->FindGroup(groupValue);
        uint64_t maxSize = group_number->maximum_size();
        uint64_t total = 0;
        auto partitions = builder->ListPartitionsInGroup(groupValue);
        for (const auto& partition : partitions) {
            cout << "" << endl;
            float size_mb = std::round((partition->BytesOnDisk() / 1024.0 / 1024.0) * 100) / 100.0;
            if ( size_mb == 0 ) {
                size_mb = std::round((partition->BytesOnDisk() / 1024.0) * 100) / 100.0;
                std::cout << partition->name() << ":" << partition->BytesOnDisk() << ":" << size_mb << "KB" << std::endl;
            } else {
                std::cout << partition->name() << ":" << partition->BytesOnDisk() << ":" << size_mb << "MB" << std::endl;
            }
            total += partition->BytesOnDisk();
        }
        uint64_t groupAllocatable = maxSize - total;
        cout << "" << endl;
        // std::cout << builder->AllocatableSpace() << ":" << builder->UsedSpace() << std::endl;
        uint64_t superFreeSpace = builder->AllocatableSpace() - builder->UsedSpace();
        if(groupAllocatable > superFreeSpace || maxSize == 0)
            groupAllocatable = superFreeSpace;

        printf("Free space: %" PRIu64 "\n", groupAllocatable);

        exit(0);

    } else if (arguments[0] == "--get-info" ) {
        if (arguments.size() != 1) {
            std::cout << "--get-info" << std::endl;
            exit(1);
        }
        
        auto groups = builder->ListGroups();
        for(auto groupName: groups) {
            auto partitions = builder->ListPartitionsInGroup(groupName);
            if ( groupName == groupValue ) {
                cout << "" << endl;
                cout << "GroupInSuper->" << groupName << " Usage->" << builder->UsedSpace() << " TotalSpace->" << builder->AllocatableSpace() << endl;
                for (const auto& partition : partitions) {
                    cout << "NamePartInGroup->" << partition->name() << " Size->" << partition->BytesOnDisk() << endl;
                }
            }
            
        }

    } else if (arguments[0] == "--unlimited-group" ) {
        if (arguments.size() != 1) {
            std::cout << "--unlimited-group" << std::endl;
            exit(1);
        }
        builder->ChangeGroupSize(groupValue, 0);
        saveToDisk(std::move(builder));
        return 0;
    } else if (arguments[0] == "--clear-cow" ) {
#ifndef LPTOOLS_STATIC
        // Ensure this is a V AB device, and that no merging is taking place (merging? in gsi? uh)
        auto svc1_1 = ::android::hardware::boot::V1_1::IBootControl::tryGetService();
        if(svc1_1 == nullptr) {
            std::cerr << "Couldn't get a bootcontrol HAL. You can clear cow only on V AB devices" << std::endl;
            return 1;
        }
        auto mergeStatus = svc1_1->getSnapshotMergeStatus();
        if(mergeStatus != ::android::hardware::boot::V1_1::MergeStatus::NONE) {
            std::cerr << "Merge status is NOT none, meaning a merge is pending. Clearing COW isn't safe" << std::endl;
            return 1;
        }
#endif

        uint64_t superFreeSpace = builder->AllocatableSpace() - builder->UsedSpace();
        std::cerr << "Super allocatable " << superFreeSpace << std::endl;

        uint64_t total = 0;
        auto partitions = builder->ListPartitionsInGroup("cow");
        for (const auto& partition : partitions) {
            std::cout << "Deleting partition? " << partition->name() << std::endl;
            if(ends_with(partition->name(), "-cow")) {
                std::cout << "Deleting partition " << partition->name() << std::endl;
                builder->RemovePartition(partition->name());
            }
        }
        if(!saveToDisk(std::move(builder))) {
            std::cerr << "Failed to write partition table" << std::endl;
            exit(1);
        }
        return 0;

    } else {
        Help_menu();
        exit(1);
    }

    return 0;
}
