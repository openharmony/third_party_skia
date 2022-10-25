echo "======================================"
echo ""
echo "STEP 1: make new folder"
echo ""
echo "======================================"

currentdate=$(date +%Y%m%d_%H%M%S)
rootFolder="OpenHarmony_$currentdate"
mkdir $rootFolder
cd $rootFolder
mkdir L2
cd L2


echo "======================================"
echo ""
echo "STEP 2: download system components"
echo ""
echo "======================================"
repo init -u git@gitee.com:openharmony/manifest.git -b master --no-repo-verify
repo sync -c
repo forall -c 'git lfs pull' 

echo "======================================"
echo ""
echo "STEP 3: prebuild system components"
echo ""
echo "======================================"

build/prebuilts_download.sh

echo "======================================"
echo ""
echo "STEP 4: add third_party_skia component"
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
echo "STEP 5: compile third_party_skia"
echo ""
echo "======================================"

./build.sh --product-name rk3568  --build-target third_party_skia