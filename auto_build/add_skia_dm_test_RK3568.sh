echo -e "
    \033[31m buildConfig \033[0m
    \033[32m recover \033[0m 
"
back_file="./productdefine/common/inherit/rich_back.json"
read -p "请输入你的选择buildConfig|recover:" char
case $char in
buildConfig)
  if [ -e "$back_file" ]; then
    echo "已备份且配置"
  else
    echo "正在备份"
    cp ./productdefine/common/inherit/rich.json ./productdefine/common/inherit/rich_back.json
    cp ./base/hiviewdfx/hilog/interfaces/js/kits/napi/BUILD.gn ./base/hiviewdfx/hilog/interfaces/js/kits/napi/BUILD_back.gn
    cp ./developtools/profiler/hidebug/interfaces/js/kits/napi/BUILD.gn ./developtools/profiler/hidebug/interfaces/js/kits/napi/BUILD_back.gn
    cp ./base/hiviewdfx/hichecker/interfaces/js/kits/napi/BUILD.gn ./base/hiviewdfx/hichecker/interfaces/js/kits/napi/BUILD_back.gn
    cp ./foundation/resourceschedule/ffrt/BUILD.gn ./foundation/resourceschedule/ffrt/BUILD_back.gn
    cp ./third_party/skia/BUILD.gn ./third_party/skia/BUILD_back.gn
    cp ./third_party/skia/bundle.json ./third_party/skia/bundle_back.json
    cp ./foundation/communication/netstack/interfaces/kits/c/net_ssl/BUILD.gn ./foundation/communication/netstack/interfaces/kits/c/net_ssl/BUILD_back.gn
    echo "备份完成"
    echo "======================================"
    echo ""
    echo "STEP 1: add skia component"
    echo ""
    echo "======================================"

    target_file="./productdefine/common/inherit/rich.json"
    sed -i '/"thirdparty"/{n;d}' ${target_file}

    target_line='   "components": [\
        {\
          "component": "skia",\
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

    api_target_file="./base/hiviewdfx/hichecker/interfaces/js/kits/napi/BUILD.gn"
    api_target_line='output_name = "napihichecker"'
    sed -i "/ohos_shared_library/a\  ${api_target_line}" ${api_target_file}

    api_target_file="./foundation/resourceschedule/ffrt/BUILD.gn"
    api_target_line='output_name = "libffrt_ndk"'
    sed -i "/ohos_shared_library/a\  ${api_target_line}" ${api_target_file}

    api_target_file="./third_party/skia/BUILD.gn"
    sed -i 's/^.*"-flto=thin".*/#&/' ${api_target_file}
    sed -i 's/^.*"-fvisibility=hidden".*/#&/' ${api_target_file}
    sed -i 's/^.*"-fvisibility-inlines-hidden".*/#&/' ${api_target_file}

    api_target_file="./foundation/communication/netstack/interfaces/kits/c/net_ssl/BUILD.gn"
    sed -i 's/^.*output_name = "net_ssl".*/#&/' ${api_target_file}
    sed -i 's/^.*output_extension = "so".*/#&/' ${api_target_file}

    api_target_line='output_name = "net_ssl"'
    sed -i "/^.*libnet_ssl_ndk/a\  ${api_target_line}" ${api_target_file}

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
  fi
  ;;
recover)
  if [ -e "$back_file" ]; then
    mv ./productdefine/common/inherit/rich_back.json ./productdefine/common/inherit/rich.json
    mv ./base/hiviewdfx/hilog/interfaces/js/kits/napi/BUILD_back.gn ./base/hiviewdfx/hilog/interfaces/js/kits/napi/BUILD.gn
    mv ./developtools/profiler/hidebug/interfaces/js/kits/napi/BUILD_back.gn ./developtools/profiler/hidebug/interfaces/js/kits/napi/BUILD.gn
    mv ./base/hiviewdfx/hichecker/interfaces/js/kits/napi/BUILD_back.gn ./base/hiviewdfx/hichecker/interfaces/js/kits/napi/BUILD.gn
    
    mv ./foundation/resourceschedule/ffrt/BUILD_back.gn ./foundation/resourceschedule/ffrt/BUILD.gn
    mv ./third_party/skia/BUILD_back.gn ./third_party/skia/BUILD.gn
    mv ./third_party/skia/bundle_back.json ./third_party/skia/bundle.json
    mv ./foundation/communication/netstack/interfaces/kits/c/net_ssl/BUILD_back.gn ./foundation/communication/netstack/interfaces/kits/c/net_ssl/BUILD.gn
    echo "恢复完成"
  else
    echo "没有备份的文件可恢复"
  fi
  ;;
*)
  echo "输入不正确！请重新输入。"
  ;;
esac
