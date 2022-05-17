function [y] = rendercost(x)

w = floor(1920 * 0.555);
h = floor(1080 * 0.555);
r = round(sqrt(w*w + h*h) * x/2);
L = log(sqrt(w*w + h*h) / 2);
u = floor((max(log(r), 0)/L)^(1/4) * w);

AL = (w - u) * h;
AR = r*r*pi;
AS = 1920*1080;
AD = w*h;

y = min(AL + AR, AD)/AS;

end

