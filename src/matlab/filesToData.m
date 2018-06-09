function [sensors] = filesToData(folder)

%If no arguments, select the folder
if nargin<1
   folder = uigetdir;
end
fprintf('\nFolder selected:\n %s\n', folder)

%Read general info
fprintf('Searching devices...\n')
filepath = strcat(folder, '/deviceinfo.csv');
data = readtable(filepath);


for i = 1:1:height(data)
    fprintf('Found device: %s\n',data{i,1}{1})
    sensor.id = data{i,1}{1};
    sensor.name = data{i,2};
    sensor.fs = data{i,3};
    sensor.packets = data{i,4};
    str = sprintf("/data/%s.csv",sensor.id);
    datapath = strcat(folder, str);
    dat = readtable(datapath);
    sensor.data = dat; 
    sensors(i) = sensor;
end

fprintf('\n%d devices loaded\n',height(data));
end