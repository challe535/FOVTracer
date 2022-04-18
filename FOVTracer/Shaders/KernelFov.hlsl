

float kernelFuncInv(float x, float a)
{
    return pow(x, abs(1 / a));
}

float kernelFunc(float x, float a)
{
    return pow(x, abs(a));
}