# anisotropinator

Simple utility created for us to evaluate encoding anisotropy texture data in 2 channels, with xy representing a 2D vector and strength encoded as the magnitude of the vector.

> **Warning**
> This is a live document, research is all a work in progress.

# Comparisons

Below is the sample model to test anisotropy with varied roughness, as well as a gradient test to assess any information loss in the conversion process due to quantization (using less bits to represent strength and direction).

![BabylonReference](images/babylon.reference.png)

*AnisotropyGradientsRoughness Model created by @ericchadwick, rendered in Babylon.js*

## "Default" Anisotropy Representation
![Input](images/AnisotropyGradientsRoughness_img0.png)

*Input format: Red,Green channels specify a 2D direction, Blue channel defines anisotropy strength with values ranging from 0 (no anisotropy) to 1 (full anisotropy).*

## 2 Channel Anisotropy Representation
![Input](images/AnisotropyGradientsRoughness_img0.2D.png)

*2D format: anistropy is encoded as a 2D direction, with the magnitude indicating the strength.*

## Roundtrip 2 Channel back into 3 Channel
![Input](images/AnisotropyGradientsRoughness_img0.2D.3channel.png)

*2channel anistropy transformed back into the "default" 3 channel representation.*

## Roundtrip 2 Channel into Angular Direction
![Input](images/AnisotropyGradientsRoughness_img0.2D.angle.png)

*2 channel anistropy (2D direciton with magnitude representing strength) transformed into a 2 channel angle, strength representation.*

# Renderings

The following are preliminary renderings in my own renderer to test out the 3 channel vs 2 channel representation.

|                      3 channel                       |                   2 channel                    |
| :--------------------------------------------------: | :--------------------------------------------: |
|     ![](images/anisotropyBarnLamp.3channel.png)      |     ![](images/anisotropyBarnLamp.2D.png)      |
| ![](images/anisotropyGradientRoughness.3channel.png) | ![](images/anisotropyGradientRoughness.2D.png) |

## AnisotropyGradientRoughness Breakdown

|    Output     |                      3 channel                       |                   2 channel                    |
| :-----------: | :--------------------------------------------------: | :--------------------------------------------: |
|    normals    |     ![](images/agr_normals.3channel.png)      |     ![](images/agr_normals.2D.png)      |
|     dirx      | ![](images/agr_dirx.3channel.png) | ![](images/agr_dirx.2D.png) |
|     diry      | ![](images/agr_diry.3channel.png) | ![](images/agr_diry.2D.png) |
|      str      | ![](images/agr_str.3channel.png) | ![](images/agr_str.2D.png) |
| anisotropicT  | ![](images/agr_anisotropicT.3channel.png) | ![](images/agr_anisotropicT.2D.png) |
| specular_brdf | ![](images/agr_specular_brdf.3channel.png) | ![](images/agr_specular_brdf.2D.png) |

# Implementation

Decoding the 2 channel anisotropy goes as follows:

```GLSL
    vec2 anisotropySample = texture(u_AnisotropyTexture, texCoords).rg;
    vec3 direction = vec3(anisotropySample.rg * 2.0 - vec2(1.0), 0.0);
    float anisotropyStrength = length(direction.rg) * pbrInputs.anisotropy.b;
    if (dot(anisotropySample, anisotropySample) == 0.0)
    {
        direction.rg = pbrInputs.anisotropy.rg;
        anisotropyStrength = pbrInputs.anisotropy.b;
    }
    direction = normalize(direction);
    anisotropy = clamp(anisotropyStrength, 0.0, 1.0);
    anisotropicT = normalize(getTBN() * direction);
    vec3 anisotropicB = normalize(cross(n, anisotropicT));
    specularBrdf = specular_brdf_anisotropic(alphaRoughness, anisotropy, n, v, l, h, anisotropicT, anisotropicB);
```

BRDF implementation:

```GLSL
// GGX Distribution Anisotropic
// https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf Addenda
float D_GGX_anisotropic(float NdotH, float TdotH, float BdotH, float anisotropy, float at, float ab)
{
    float a2 = at * ab;
    vec3 f = vec3(ab * TdotH, at * BdotH, a2 * NdotH);
    float w2 = a2 / dot(f, f);
    return a2 * w2 * w2 / c_Pi;
}

// GGX Mask/Shadowing Anisotropic
// Heitz http://jcgt.org/published/0003/02/03/paper.pdf
float V_GGX_anisotropic(float NdotL, float NdotV, float BdotV, float TdotV, float TdotL, float BdotL, float at, float ab)
{
    float GGXV = NdotL * length(vec3(at * TdotV, ab * BdotV, NdotV));
    float GGXL = NdotV * length(vec3(at * TdotL, ab * BdotL, NdotL));
    float v = 0.5 / (GGXV + GGXL);
    return clamp(v, 0.0, 1.0);
}

float specular_brdf_anisotropic(float alphaRoughness, float anisotropy, vec3 n, vec3 v, vec3 l, vec3 h, vec3 t, vec3 b)
{
    float at = max(alphaRoughness * (1.0 + anisotropy), 0.00001);
    float ab = max(alphaRoughness * (1.0 - anisotropy), 0.00001);

    float V = V_GGX_anisotropic(dot(n, l), dot(n, v), dot(b, v), dot(t, v), dot(t, l), dot(b, l), at, ab);
    float D = D_GGX_anisotropic(clampdotEps(n, h), dot(t, h), dot(b, h), anisotropy, at, ab);

    return V * D;
}
```