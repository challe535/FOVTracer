n = 100000;
x = 0:1/n:1;
y = movmean(arrayfun(@(e) rendercost(e), x), 500);

x = x*100;

norm_y = (y - min(y)) / (max(y) - min(y));
p = plot(x, norm_y);
p(1).Color = [140,150,198]/255;

hold on;

tot_times = movmean(readmatrix('varying_fov_threshold_100_000\tot_times.txt'), 500);

norm_tot = (tot_times - min(tot_times)) / (max(tot_times) - min(tot_times));
s = plot(x, norm_tot);
s(1).Color = [136,86,167]/255;

[~,i] = max(-tot_times);
l = xline(100*i/n, '--', "Min = " + 100*i/n + "%");
l.Color = [129,15,124]/255;

legend('Estimated', 'Observed');
xlabel('Foveation threshold (%)')
ylabel('Normalized render time')
title('Normalized render time pattern for \alpha = 4');

hold off;