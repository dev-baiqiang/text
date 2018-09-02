#version 330 core
in vec2 TexCoord;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;
uniform float t_width;
uniform float t_height;
uniform int blur;

float offset[4] = float[]( 0.0, 1.3846153846, 3.2307692308, 4.2307692308 );
float weight[4] = float[]( 0.2270270270, 0.4162162162, 0.1162162162, 0.0702702703 );

vec4 getColor(vec2 coords) {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, coords).r);
    return vec4(textColor, 1.0) * sampled;
}

void main()
{
    if (blur == 1) {
        vec2 uv = TexCoord;
        vec4 tc = getColor(uv) * weight[0];
        for(int i = 1; i < 4; i++) {
            tc += getColor(uv + vec2(0.0, offset[i]) / t_height) * weight[i];
            tc += getColor(uv - vec2(0.0, offset[i]) / t_height) * weight[i];
        }
        color = vec4(0.0, 0.0, 0.0, 1.0) * tc;
    } else {
        color = getColor(TexCoord);
    }
}