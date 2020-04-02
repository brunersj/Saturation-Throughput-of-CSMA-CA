%% EE597 Project
% Charlie Andre / Sam Bruner
clc;
clear;
close all;
%% Parameters from Bianchi Model

payload_size = 8184;
MAC_header = 272;
PHY_header = 128;
ACK_size = 112 + PHY_header;
% RTS/CTS Omitted
Ch_bit_rate = 1; %Mbps
prop_delay = 1;% us
SIFS = 28; %us
slot_time = 50; %us
DIFS = 128; %us
H = PHY_header + MAC_header;
packet_size = payload_size + H;

% Average time the channel is sensed busy because of a successful transmission
Ts = (H + payload_size + SIFS + prop_delay + ACK_size + DIFS + prop_delay) / slot_time;

% Average time the channel is sensed busy by each station during a collision
Tc = (H + payload_size + DIFS + prop_delay) / slot_time;

Wvec = [32,32,128];
mvec = [3,5,3];

% Saturation throughput for each simulation
S = zeros(3,7);
% Number of stations
nodes = [5,10,15,20,25,30,50];

%% Run Simulation for varying CWmin/max - Solve saturation throughput from process in Bianchi's paper
for plotnum = 1:3
    W = Wvec(plotnum);
    m = mvec(plotnum);
    for n = 1:length(nodes)
        node = nodes(n);
        % (p-1 + (1 - 2(1-2p) / ((1-2p)(W+1) + pW(1-(2p)^m)))^(n-1)
        SolveForP = @(p) (1-p - (1 - 2*(1-2*p) / ((1-2*p)*(W+1)+ p*W*(1-(2*p)^m)) )^(node-1));
        
        % probability p that a transmitted packet collides
        % p = 1 - (1 - tau)^node-1 
        % plug in tau and solve for p
        % p = 1 - (1 - 2*(1-2*p) / ((1-2*p)*(W+1)+ p*W*(1-(2*p)^m)) )^node-1;
        p = fsolve(SolveForP,0);
        
        % Each node transmits in a randomly chosen time slot w.p. tau
        tau = 2*(1 - 2*p) / ((1 - 2*p)*(W + 1)+ p*W*(1 - (2*p)^m)); 

        % Ptr: probability that there is at least one transmission in the considered slot time
        Ptr = 1 - (1 - tau) ^ node; 
        % Ps is the probability that a transmission is successful
        Ps = node * tau * (1 - tau) ^ (node - 1) / Ptr;
        % S = E[payload information transmitted in a slot time] / E[length of a slot time]
        S(plotnum, n) = (Ps * Ptr * payload_size/slot_time) / ( (1 - Ptr) * prop_delay + Ptr*Ps*Ts + Ptr*(1 - Ps)*Tc);
    end
end

%% Plot Saturation Throughput
hold on;
plot(nodes,S(1,:),'-^');
plot(nodes,S(2,:), '-s');
plot(nodes,S(3,:), '-o');
hold off;
title('Matlab Saturation Throughput Simulation')
xlabel('Number of Stations');
ylabel('Saturation Throughput');
legend('W=32, m=3', 'W=32, m=5', 'W=128, m=3');