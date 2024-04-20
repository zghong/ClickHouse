#pragma once
#include "config.h"

#if USE_HDFS
#include <Storages/ObjectStorage/StorageObjectStorageConfiguration.h>
#include <Interpreters/Context_fwd.h>
#include <Parsers/ASTFunction.h>
#include <Parsers/IAST_fwd.h>

namespace DB
{

class StorageHDFSConfiguration : public StorageObjectStorageConfiguration
{
public:
    static constexpr auto type_name = "hdfs";
    static constexpr auto engine_name = "HDFS";

    StorageHDFSConfiguration() = default;
    StorageHDFSConfiguration(const StorageHDFSConfiguration & other);

    std::string getTypeName() const override { return type_name; }
    std::string getEngineName() const override { return engine_name; }

    Path getPath() const override { return path; }
    void setPath(const Path & path_) override { path = path_; }

    const Paths & getPaths() const override { return paths; }
    Paths & getPaths() override { return paths; }
    void setPaths(const Paths & paths_) override { paths = paths_; }

    String getNamespace() const override { return ""; }
    String getDataSourceDescription() override { return url; }
    StorageObjectStorage::QuerySettings getQuerySettings(const ContextPtr &) const override;

    void check(ContextPtr context) const override;
    ObjectStoragePtr createObjectStorage(ContextPtr context, bool is_readonly = true) override; /// NOLINT
    StorageObjectStorageConfigurationPtr clone() override { return std::make_shared<StorageHDFSConfiguration>(*this); }

    void addStructureAndFormatToArgs(
        ASTs & args, const String & structure_, const String & format_, ContextPtr context) override;

    std::string getPathWithoutGlob() const override;

private:
    void fromNamedCollection(const NamedCollection &) override;
    void fromAST(ASTs & args, ContextPtr, bool /* with_structure */) override;
    void setURL(const std::string & url_);

    String url;
    String path;
    std::vector<String> paths;
};

}

#endif
