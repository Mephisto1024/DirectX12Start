struct VertexOut
{
    float4 PosH : SV_Position; //Homoogeneous
    float4 Color : COLOR;
};
float4 main(VertexOut pin) : SV_TARGET    //��ʾ�÷���ֵ������rt��ʽƥ��
{
	return pin.Color;
}