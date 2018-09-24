#include "testutils/Load_Test.h"
#include <gitversion/gitversion.h>
#include "testutils/FilesystemHelper.h"

using cryfs::CryConfig;
using cryfs::CryConfigFile;
using cryfs::CryDevice;
using blockstore::ondisk::OnDiskBlockStore2;
using cpputils::unique_ref;
using cpputils::make_unique_ref;
using cpputils::TempDir;
using cpputils::TempFile;
using cpputils::Random;
using cpputils::AES256_GCM;
using cpputils::SCrypt;
using boost::optional;
using boost::none;
using std::shared_ptr;

namespace bf = boost::filesystem;

class Load_Test_With_Filesystem : public Load_Test {
public:
    Load_Test_With_Filesystem(): basedir(), externalconfig(false) {}

    void remove_all_blocks_in(const bf::path &dir) {
        for (bf::directory_iterator iter(dir); iter != bf::directory_iterator(); ++iter) {
            if (iter->path().filename() != "cryfs.config") {
                bf::remove_all(iter->path());
            }
        }
    }

    void set_basedir(const optional<bf::path> &_basedir = none) {
        bf::path actual_basedir = basedir.path();
        if (_basedir != none) {
            actual_basedir = *_basedir;
        }
        EXPECT_SUCCESS(cryfs_load_set_basedir(context, actual_basedir.native().c_str(), actual_basedir.native().size()));
    }

    void set_password(const string &password = PASSWORD) {
        EXPECT_SUCCESS(cryfs_load_set_password(context, password.c_str(), password.size()));
    }

    void set_externalconfig() {
        EXPECT_SUCCESS(cryfs_load_set_externalconfig(context, externalconfig.path().native().c_str(), externalconfig.path().native().size()));
    }

    cryfs_mount_handle *load_with_externalconfig() {
        set_basedir();
        set_password();
        set_externalconfig();
        cryfs_mount_handle *handle = nullptr;
        EXPECT_SUCCESS(cryfs_load(context, &handle));
        return handle;
    }

    cryfs_mount_handle *load_with_internalconfig() {
        set_basedir();
        set_password();
        cryfs_mount_handle *handle = nullptr;
        EXPECT_SUCCESS(cryfs_load(context, &handle));
        return handle;
    }

    void EXPECT_CIPHER_IS(const std::string &expectedCipher, cryfs_mount_handle *handle) {
        const char *actualCipher = nullptr;
        EXPECT_SUCCESS(cryfs_mount_get_ciphername(handle, &actualCipher));
        EXPECT_EQ(expectedCipher, std::string(actualCipher));
    }

    TempDir basedir;
    TempFile externalconfig;
};

TEST_F(Load_Test_With_Filesystem, setup) {
        //Do nothing, just test that the file system can be setup properly
}

TEST_F(Load_Test_With_Filesystem, load) {
    create_filesystem(basedir.path());
    set_basedir();
    set_password();
    EXPECT_LOAD_SUCCESS();
}

TEST_F(Load_Test_With_Filesystem, load_withexternalconfig) {
    create_filesystem(basedir.path(), externalconfig.path());
    set_basedir();
    set_externalconfig();
    set_password();
    EXPECT_LOAD_SUCCESS();
}

TEST_F(Load_Test_With_Filesystem, load_without_getting_mounthandle) {
    create_filesystem(basedir.path());
    set_basedir();
    set_password();
    EXPECT_SUCCESS(cryfs_load(context, nullptr));
}

TEST_F(Load_Test_With_Filesystem, load_wrongpassword) {
    create_filesystem(basedir.path());
    set_basedir();
    set_password("wrong_password");
    EXPECT_LOAD_ERROR(cryfs_error_DECRYPTION_FAILED);
}

TEST_F(Load_Test_With_Filesystem, load_wrongpassword_withexternalconfig) {
    create_filesystem(basedir.path(), externalconfig.path());
    set_basedir();
    set_externalconfig();
    set_password("wrong_password");
    EXPECT_LOAD_ERROR(cryfs_error_DECRYPTION_FAILED);
}

TEST_F(Load_Test_With_Filesystem, load_missingrootblob) {
    create_filesystem(basedir.path());
    remove_all_blocks_in(basedir.path());
    set_basedir();
    set_password();
    EXPECT_LOAD_ERROR(cryfs_error_FILESYSTEM_INVALID);
}

TEST_F(Load_Test_With_Filesystem, load_missingrootblob_withexternalconfig) {
    create_filesystem(basedir.path(), externalconfig.path());
    remove_all_blocks_in(basedir.path());
    set_basedir();
    set_externalconfig();
    set_password();
    EXPECT_LOAD_ERROR(cryfs_error_FILESYSTEM_INVALID);
}

TEST_F(Load_Test_With_Filesystem, load_missingconfigfile) {
    create_filesystem(basedir.path(), externalconfig.path());
    bf::remove(basedir.path() / "cryfs.config");
    set_basedir();
    set_password();
    EXPECT_LOAD_ERROR(cryfs_error_CONFIGFILE_DOESNT_EXIST);
}

TEST_F(Load_Test_With_Filesystem, load_missingconfigfile_withexternalconfig) {
    create_filesystem(basedir.path(), externalconfig.path());
    set_basedir();
    set_externalconfig();
    externalconfig.remove();
    set_password();
    EXPECT_LOAD_ERROR(cryfs_error_CONFIGFILE_DOESNT_EXIST);
}

TEST_F(Load_Test_With_Filesystem, load_incompatible_version) {
    create_filesystem(basedir.path());
    create_configfile_for_incompatible_cryfs_version(externalconfig.path());
    set_basedir();
    set_externalconfig();
    set_password();
    EXPECT_LOAD_ERROR(cryfs_error_FILESYSTEM_INCOMPATIBLE_VERSION);
}

TEST_F(Load_Test_With_Filesystem, load_takescorrectconfigfile_onlyinternal) {
    create_filesystem(basedir.path(), none, "aes-256-gcm"); // passing none as config file creates an internal config file
    auto handle = load_with_internalconfig();
    EXPECT_CIPHER_IS("aes-256-gcm", handle);
}

TEST_F(Load_Test_With_Filesystem, load_takescorrectconfigfile_onlyexternal) {
    create_filesystem(basedir.path(), externalconfig.path(), "aes-256-gcm");
    auto handle = load_with_externalconfig();
    EXPECT_CIPHER_IS("aes-256-gcm", handle);
}

TEST_F(Load_Test_With_Filesystem, load_takescorrectconfigfile_takeexternal) {
    create_filesystem(basedir.path(), none, "aes-256-gcm");
    create_configfile(externalconfig.path(), "twofish-256-cfb");
    auto handle = load_with_externalconfig();
    EXPECT_CIPHER_IS("twofish-256-cfb", handle);
}

TEST_F(Load_Test_With_Filesystem, load_takescorrectconfigfile_takeinternal) {
    create_filesystem(basedir.path(), none, "aes-256-gcm");
    create_configfile(externalconfig.path(), "twofish-256-cfb");
    auto handle = load_with_internalconfig();
    EXPECT_CIPHER_IS("aes-256-gcm", handle);
}
