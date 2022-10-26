echo "======================================"
echo ""
echo "STEP 1: add third_party_skia component"
echo ""
echo "======================================"

target_file="./productdefine/common/inherit/rich.json"
sed -i '/"thirdparty"/{n;d}' ${target_file}

target_line='   "components": [\
        {\
          "component": "third_party_skia",\
          "features": []\
        },'

key_line='"thirdparty"'

sed -i "/${key_line}/a\   ${target_line}" ${target_file}

echo "======================================"
echo ""
echo "    STEP 2: modify napi api"
echo ""
echo "======================================"

api_target_gn_file="./base/hiviewdfx/hilog/interfaces/js/kits/napi/BUILD.gn"

sed -i '/output_name/s/libhilog/libhilognapi/g' ${api_target_gn_file}

api_target_file="./developtools/profiler/hidebug/interfaces/js/kits/napi/BUILD.gn"
api_target_line='output_name = "libhidebugnapi"'
sed -i "/ohos_shared_library/a\  ${api_target_line}" ${api_target_file}

echo "======================================"
echo ""
echo "   STEP 3: add dm compilation"
echo ""
echo "======================================"

skia_bundle_file="./third_party/skia/bundle.json"

skia_pivot_line='inner_kits'
skia_target_line='"test": [ "//third_party/skia:dm(//build/toolchain/ohos:ohos_clang_arm)" ]'

sed -i '/inner_kits/{n;d}' ${skia_bundle_file}
sed -i "/${skia_pivot_line}/a\            ${skia_target_line}" ${skia_bundle_file}
