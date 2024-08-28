cbuffer cbPerObject : register(b0)
{
    float4x4 matModel;
}
cbuffer cbPass : register(b1)
{
    float4x4 matView;
    float4x4 matProj;
    float4x4 matViewProj;
}
struct VertexIn
{
    float3 PosL : Position;    //local
    float4 Color : COLOR;
};
struct VertexOut
{
    float4 PosH : SV_Position;    //Homoogeneous
    float4 Color : COLOR;
};
VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), matModel);
    vout.PosH = mul(posW, matViewProj);
    
    vout.Color = vin.Color;
    
	return vout;
}