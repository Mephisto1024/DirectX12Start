struct VertexOut
{
    float4 PosH : SV_Position; //Homoogeneous
    float4 Color : COLOR;
};
float4 main(VertexOut pin) : SV_TARGET    //表示该返回值类型与rt格式匹配
{
	return pin.Color;
}