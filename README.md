# anisotropinator

Simple utility created for us to evaluate encoding anisotropy texture data in 2 channels, with xy representing a 2D vector and strength encoded as the magnitude of the vector.

> **Warning**
> There appears to be a bug in either my rendering of anisotropy, or this conversion tool itself based on the results shown below. This is a live document.

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
    if (dot(direction, direction) == 0.0)
    {
        direction.rg = pbrInputs.anisotropy.rg;
    }
    float anisotropyStrength = length(direction.rg);
    direction = normalize(direction);
    anisotropy = clamp(anisotropyStrength * pbrInputs.anisotropy.b, 0.0, 1.0);
    anisotropicT = normalize(getTBN() * direction);
    vec3 anisotropicB = normalize(cross(n, anisotropicT));
    specularBrdf = specular_brdf_anisotropic(alphaRoughness, anisotropy, n, v, l, h, anisotropicT, anisotropicB);
```
