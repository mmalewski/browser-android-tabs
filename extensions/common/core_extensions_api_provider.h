// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_
#define EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_

#include "base/macros.h"
#include "extensions/common/extensions_api_provider.h"
#include "extensions/common/permissions/extensions_api_permissions.h"

namespace extensions {

class CoreExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  CoreExtensionsAPIProvider();
  ~CoreExtensionsAPIProvider() override;

  // ExtensionsAPIProvider:
  void AddAPIFeatures(FeatureProvider* provider) override;
  void AddManifestFeatures(FeatureProvider* provider) override;
  void AddPermissionFeatures(FeatureProvider* provider) override;
  void AddBehaviorFeatures(FeatureProvider* provider) override;
  void AddAPIJSONSources(JSONFeatureProviderSource* json_source) override;
  bool IsAPISchemaGenerated(const std::string& name) override;
  base::StringPiece GetAPISchema(const std::string& name) override;
  void AddPermissionsProviders(PermissionsInfo* permissions_info) override;
  void RegisterManifestHandlers() override;

 private:
  const ExtensionsAPIPermissions api_permissions_;

  DISALLOW_COPY_AND_ASSIGN(CoreExtensionsAPIProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CORE_EXTENSIONS_API_PROVIDER_H_
