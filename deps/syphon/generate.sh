#!/bin/sh

# Adapted from the Xcode project.

set -e -x
cd `dirname ${0}`

mkdir -p "generated/Syphon"
cp "Syphon-Framework/Syphon.h" "generated/Syphon/"

for header in "SyphonClient" "SyphonServer" "SyphonServerDirectory" "SyphonImage"
do
    sed 's:#import://SYPHON_IMPORT_PLACEHOLDER:' \
        "Syphon-Framework/${header}.h" \
        > "${header}_stage_1.h"

    gcc -E -C -P -nostdinc \
        -D "SYPHON_HEADER_BUILD_PHASE" \
        -include "Syphon-Framework/SyphonBuildMacros.h" \
        "${header}_stage_1.h" \
        -o "${header}_stage_2.h"

    sed 's://SYPHON_IMPORT_PLACEHOLDER:#import:' \
        "${header}_stage_2.h" \
        > "${header}_stage_3.h"

    sed '/./,$!d' "${header}_stage_3.h" \
        > "${header}_stage_4.h"

    cat -s "${header}_stage_4.h" \
        > "generated/Syphon/${header}.h"

    rm  "${header}_stage_1.h" \
        "${header}_stage_2.h" \
        "${header}_stage_3.h" \
        "${header}_stage_4.h"
done
