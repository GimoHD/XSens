% Get the name of the user who logged in to the computer.
userProfile = getenv('USERPROFILE')
% Create a string to the "My Documents" folder of this Windows user:
docs = sprintf('%s\\Documents\\Awinda Monitor', userProfile)

dname = uigetdir(docs);

%Get sensordata from current folder
sensors = filesToData(dname);
