module;
#include <cstdint>
export module SettingMod;

export struct SettingInfo
{
	bool UseTileBasedLighting;
	bool UseFXAA;
	struct AOSettingInfo
	{
		enum class AO : int32_t
		{
			None = 0,
			SSAO = 1,
			HBAO = 2,
			GTAO = 3
		};
		int32_t UseAO{ 0 };
		float HBAOTangentBias{ 30.0f };
		float Strength{ 1.90f };

		float Radius{ 0.1f };
		float MaxRadiusPixels{ 100.0f };
		int NumDir{ 6 };
		int NumSteps{ 4 };
		int NumSamples{ 30 };
		bool UseCBFBlur{ false };
		float BlurKernelRadius{ 8.0f };
		float ZStrength{ 30.0f };

	} AOSetting;
	struct AASettingInfo
	{
		enum class AA : uint32_t
		{
			None = 0,
			FXAA = 1,
		};
		bool UseAA;
		float FXAAEdgeThreshold;
	} AASetting;
	struct PostSettingInfo
	{
		bool UseBloom{ false };
		float BloomScale{ 1.0f };
		float BloomStength{ 1.0f };
		float Gamma{ 2.2 };
		float Exposure{ 4.5 };
	} PostSetting;
	struct ShadowSettingInfo
	{

		bool PointLightShadow{ false };
		bool DirLightShadow{ false };
		bool DirLightPCF{ false };
		bool SpotLightShadow{ false };
		bool DirLight{ false };
		bool PointLight{ false };
		bool SpotLight{ false };
		float DepthBiasCons{ 1.0f };
		float DepthBiasSlope{ 1.0f };
	} ShadowSetting;
	struct PBRSettingInfo
	{
		float MetallicFactor{ 1.0f };
		float RoughnessFactor{ 1.0f };
		int SkyBoxIndex{ 0 };
		bool UseIBL{ false };
	} PBRSetting;
};
