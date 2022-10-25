echo "======================================"
echo ""
echo "    start add component..."
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
echo "    modification finished"
echo ""
echo "======================================"