clc

PWM12_data = readtable('pomiary/samples_21-42-36_PWM12.5%.csv');
PWM10_data = readtable('pomiary/samples_23-26-35_PWM10%.csv');

time = PWM12_data{:,1};
data10 = PWM10_data{:,2};
data12 = PWM12_data{:,2};

% Model
input10 = 0.1;
input12 = 0.125;
s = tf('s');
k = 70;
T = 170;
delay = 5;
k10 = k/input10;
k12 = k/input12;
M10 = k10/(1+s*T)*exp(-s*delay);
M12 = k12/(1+s*T)*exp(-s*delay);
M10_response = lsim(M10, input10*ones(size(time),"like",time), time) + 25.0;
M12_response = lsim(M12, input12*ones(size(time),"like", time), time) + 23.6;

% model dyskretny
dM12 = c2d(M12, 0.5, 'tustin');
dM12_response = lsim(dM12, input12*ones(size(time),"like", time), time) + 23.6;

% Plot model ciagly
hold on;
grid on;
%plot(time, data12);
plot(time, M12_response);
stairs(time, dM12_response);

disp("ok");



















