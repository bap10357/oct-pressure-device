% Remember to adjust the working directory and file name

clc; close all; clear all;
name_file='/Volumes/T7/biodesign/projects/oct_pressure_encoder/pressure_encoder_software/pressure_encoder/data_analysis_matlab/';
open_file=	[name_file, 'data_open/'] ;
save_file=	[name_file, 'data_save/'] ;
save_as='y';
%% ==========================
% ===========================
% ===      Filename       ===
% ===========================
no1=[1]; % Subject number: 1, 2, 3...
no2=[1]; % Condition 1
no3=[1]; % Condition 2
no4=[3]; % Condition 3
% This set of scripts reads the above filename as no1_no2no3no4 (for example, 1_111).
%% ==========================
clear i j k n no_step
w=0;
for i=1:length (no1)
    for j=1:length (no2)
        for k=1:length (no3)
            for m=1:length (no4)
                r1=[num2str(no1(i))];
                r2=[num2str(no2(j))];
                r3=[num2str(no3(k))];
                r4=[num2str(no4(m))];
                w=w+1;
                %no_step(w,:)= [r1,'_', r2, r3, r4];
                no_step(w,:) = sprintf('%d_%d%d%d', no1(i), no2(j), no3(k), no4(m));
            end
        end
    end
end

% Initialize an empty array to store the middle columns
middleColumns = [];

% Loop through each image file
for j = 0:4
    for k = 1:100
        % Construct the full file name
        fileName = fullfile(open_file, no_step(w,:), '/', sprintf('%d_%d.jpg', j, k));

        % Read the image
        img = imread(fileName);

        % Crop the image to remove specified rows and columns
        croppedImage = img(5:end-30, 4:end-5);

        % Determine the middle column index
        middleColIndex = floor(size(croppedImage, 2) / 2);

        % Extract the middle column
        middleColumn = croppedImage(:, middleColIndex);

        % Flip the middle column
        middleColumn = flip(middleColumn);

        % Append the middle column to the matrix
        middleColumns = [middleColumns, middleColumn];
    end
end

% Plot the resulting matrix
figure;
imagesc(middleColumns);
colormap(gray);
colorbar;
title('Combined Middle Columns of Cropped Images');
xlabel('Image Index');
ylabel('Pixel Intensity');

if save_as=='y'
    currFig = get(0,'CurrentFigure');
    saveas (currFig, [ save_file,  'i',  no_step(w,:)  ],   'jpg'  );
    csvwrite( [save_file,'i',   no_step(w,:),'.csv'], middleColumns)
end