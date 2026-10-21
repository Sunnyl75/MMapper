// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

uniform sampler2DArray uTexture;
uniform vec4 uColor;

in vec4 vColor;
in vec3 vRoomWorld;
in vec3 vTexCoord;

out vec4 vFragmentColor;

void main()
{
    vFragmentColor = vColor * uColor * texture(uTexture, vTexCoord);
    if (uRoomLightTintPass != 0) {
        float light = roomIllumination(vRoomWorld).a;
        vFragmentColor.rgb = mix(vFragmentColor.rgb,vec3(1.0),light);
    }
}
