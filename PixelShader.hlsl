/*struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
}; 

//Texture2D    mytexture : register(t0);
//SamplerState mysampler : register(s0);

float4 main(VS_Output input) : SV_Target
{
    //return mytexture.Sample(mysampler, input.uv);
    return float4(1,1,0,1);
}
*/

struct VS_Output {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D    mytexture : register(t0);
SamplerState mysampler : register(s0);

float4 main(VS_Output input) : SV_Target
{
    return mytexture.Sample(mysampler, input.uv); 
    //return float4(1,1,0,1);
}