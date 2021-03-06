% Steven Herbst
% herbst@mit.edu

% Newton-Raphson solution of a circuit
% consisting of current source in parallel
% with a diode and a resistor

Vt=1/40;    % thermal voltage
Is=1e-14;   % reverse bias current

I=20e-3;    % supply current
gr=1/100;  % conductance of resistor

v = 0;      % initial voltage guess
e = 1e-6;   % error tolerance

dv_max = 2*Vt;  % maximum change in voltage per iteration
dv = dv_max;       % change in voltage calculated by N-R

v_list = zeros(1,floor(1/e));
count = 0;

while abs(dv)>e
    count = count + 1;
    v_list(count) = v;
    
    id = Is*(exp(v/Vt)-1);
    gd = Is*exp(v/Vt)/Vt;
       
    dv = (I-id-v*gr)/(gd+gr);
    if (abs(dv) > dv_max)
        dv = sign(dv)*dv_max;
    end
    
    v = v+dv;
end

fprintf(1,'v: %f\n',v);
fprintf(1,'id: %f\n',Is*(exp(v/Vt)-1));
fprintf(1,'ir: %f\n',v*gr);

plot(v_list(1:count));
