cmp = movmean(readmatrix('1000frames_per_mode/cmp_times.txt'), 20);
dlss = movmean(readmatrix('1000frames_per_mode/dlss_times.txt'), 20);
rt = movmean(readmatrix('1000frames_per_mode/rt_times.txt'), 20);
tot = movmean(readmatrix('1000frames_per_mode/tot_times.txt'), 20);

%%Plotting
a1 = area(dlss + cmp + rt);
a1(1).FaceColor = [179,205,227]/255;
hold on;

a2 = area(dlss + cmp);
a2(1).FaceColor = [140,150,198]/255;

a3 = area(dlss);
a3(1).FaceColor = [136,86,167]/255;

l0 = xline(0,'--',{'Foveated mode'});
l0.Color = [129,15,124]/255;

l1 = xline(1000,'--',{'DLSS mode'});
l1.Color = [129,15,124]/255;

l2 = xline(2000,'--',{'TAA mode'});
l2.Color = [129,15,124]/255;

legend('Ray trace', 'Compute', 'DLSS');
xlabel('Frames');
ylabel('Execution time (ms)');
title('Execution times of the three stages in the three modes');

hold off;

%%Avgs per mode

tot_for_avg = readmatrix('1000frames_per_mode/tot_times.txt');

fov_avg = mean(tot_for_avg(1:1000));
dlss_avg = mean(tot_for_avg(1001:2000));
taa_avg = mean(tot_for_avg(2001:3000));

display("fov: " + fov_avg);
display("dlss: " + dlss_avg);
display("taa: " + taa_avg);

