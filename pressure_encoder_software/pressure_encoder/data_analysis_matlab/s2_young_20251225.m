clc; clear all; close all
name_file='/Volumes/T7/biodesign/projects/oct_pressure_encoder/pressure_encoder_software/pressure_encoder/data_analysis_matlab/';
open_file=	[name_file, 'data_open/'] ;
save_file=	[name_file, 'data_save/'] ;
auto_load='n';
save_as='y';
%% ===================
% =========================
% ===     Filename      ===
% =========================
no1=[1];  % no people
no2=[1];  % MPH: [1] 1.8; [2] 3.6; [3] 5.4;
no3=[1];  % min walking: [1] 10; [2] 20;
no4=[3];  % min of walking: [1] first; [2] last min;
%% ==========================
clear i j k n no_step
w=0;
for i=1:length (no1)
    for j=1:length (no2)
        for k=1:length (no3)
            for m=1:length (no4)
                if (no1(i)>=10)
                    r1=[num2str(no1(i))];
                else r1=['0',num2str(no1(i))];
                end
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
% #################
% #################
% #################
clear i
for w=1: length(no_step(:,1))
% for w=1
    close all;
    % ######################################
    % ##                                  ##
    % ##                                  ##
    % ##             Deform               ##
    % ##                                  ##
    % ##                                  ##
    % ######################################
    
    % ############################################
    % ##      Take the data from OCT images     ##
    % ##      (delete the data head)            ##
    % ############################################
    clear x y yy str  deform_rawraw  deform_raw
    deform_rawraw=load([save_file,'s1_track_',   no_step(w,:), '.csv']);
    deform_raw=deform_rawraw(:,2)/200; % check the PIXEL convert MM;
    %         deform_raw=deform_rawraw(:,2);
    
    % ######################################
    % ##             Plot                 ##
    % ######################################
    subplot(3,1,1);
    plot(deform_raw,'ro-'); hold on;
    xlabel('Frame (25Hz)');
    ylabel('Deformation (mm)');
    title ('Click on both sides of section minima at either end, four points total.')
    % #####################################################
    % ##        Pick 4 points and take a section         ##
    % #####################################################
    clear  q   xy_data
    for q=1:4
        if auto_load=='y'
            center_raw=load([ save_file, 's2_xy_data_', no_step(w,:),'.csv']);
            center_data=center_raw(1:4,:);
            center=center_data(q,:);
            x=center(1);
            y=center(2);
        else
            [x, y]=ginput(1);
        end
        plot(x, y, 'bx', 'linewidth',1);hold on;
        xy_data(q,:)=[floor(x), floor(y)];
    end
    xy_data1= xy_data;
    
    [a,b]=min(   deform_raw (xy_data(1)  :xy_data(2)    )    );
    dx_start=  xy_data(1)+b-1;
    dy_start=  a;
    plot(dx_start, dy_start, 'go', 'linewidth',2);hold on;
    
    clear a b
    [a,b]=min(   deform_raw (xy_data(3)  :xy_data(4)    )    );
    dx_end=  xy_data(3)+b-1;
    dy_end=  a;
    plot(dx_end, dy_end, 'go', 'linewidth',2);hold on;
    % ######################################
    % ##                                  ##
    % ##                                  ##
    % ##             Force                ##
    % ##                                  ##
    % ##                                  ##
    % ######################################
    clear raw  force_raw
    raw=load([open_file,'f', no_step(w,:), '.csv']);  %read excel data *.xlsx
    force_raw=raw(:,2)/1000;   % check the VOLT convert Kg ;
    
    %     data_xls=xlsread(['c:\c\roho_kibler_20140801_',num2str(i),'.xlsx'],'sheet1');  %read excel data *.xlsx
    
    
    subplot(3,1,2);
    plot(force_raw,'bo-'); hold on;
    xlabel('Frame (10Hz)');
    ylabel('Force (kg)');
    title ('Click on both sides of section maxima at either end, four points total.')
    % #####################################################
    % ##        Pick 4 points and take a section         ##
    % #####################################################
    clear  q   xy_data
    for q=1:4
        if auto_load=='y'
            center_raw=load([ save_file, 's2_xy_data_', no_step(w,:),'.csv']);
            center_data=center_raw(5:8,:);
            center=center_data(q,:);
            x=center(1);
            y=center(2);
        else
            [x, y]=ginput(1);
        end
        plot(x, y, 'bx', 'linewidth',1);hold on;
        xy_data(q,:)=[floor(x), floor(y)];
    end
    xy_data2= xy_data;
    
    [a,b]=max(   force_raw (xy_data(1)  :xy_data(2)    )    );
    fx_start=  xy_data(1)+b-1;
    fy_start=  a;
    plot(fx_start, fy_start, 'go', 'linewidth',2);hold on;
    
    clear a b
    [a,b]=max(   force_raw (xy_data(3)  :xy_data(4)    )    );
    fx_end=  xy_data(3)+b-1;
    fy_end=  a;
    plot(fx_end, fy_end, 'go', 'linewidth',2);hold on;
    
    
    % ######################################################
    % ##                                                  ##
    % ##                                                  ##
    % ##           Same data length: 101 points           ##
    % ##                                                  ##
    % ##                                                  ##
    % ######################################################
    % #######################
    force=force_raw(fx_start: fx_end      );
    deform= deform_raw (dx_start: dx_end    )  ;
    % #######################
    % ========================================
    % ==    Split force into 101 points     ==
    % ========================================
    xi_divide=100;
    y=force;
    x=(0:(xi_divide/(length(y)-1)):xi_divide)';
    xi=0:(xi_divide/xi_divide):xi_divide;
    yi=interp1(x,y,xi,'linear')';
    force100_true=yi;
    force100=force100_true - min(force100_true) ;
    % ==========================================
    % ==   Split deformation into 101 points  ==
    % ==========================================
    clear x y xi yi
    
    y=deform;
    x=(0:(xi_divide/(length(y)-1)):xi_divide)';
    xi=0:(xi_divide/xi_divide):xi_divide;
    yi=interp1(x,y,xi,'linear')';
    deform100_true=yi;
    deform100=-(deform100_true - max(deform100_true) );
    % =========================================
    % ==   Split time (sec) into 101 points  ==
    % =========================================
    clear x y xi yi
    
    y=0:0.1:length(force)/10;
    x=(0:(xi_divide/(length(y)-1)):xi_divide)';
    xi=0:(xi_divide/xi_divide):xi_divide;
    yi=interp1(x,y,xi,'linear')';
    time100=yi;
    
    subplot(3,2,5);
    plot(time100, force100,'b-', 'linewidth',2); hold on;
    plot(time100, deform100,'r-'); hold on;
    h_legend=legend ('Force','Deformation' );
    set(h_legend,'FontSize',5);
    xlabel('Time (sec)');
    ylabel({'Force in kg';'Deformation in mm'});
    axis tight
    % ##########################################
    % ##                                      ##
    % ##                                      ##
    % ##           Young's Modulus            ##
    % ##                                      ##
    % ##                                      ##
    % ##########################################
    % ==========================
    subplot(3,2,6);
    plot(deform100, force100,'k-'); hold on;
    %plot(deform100(1:30), force100(1:30),'r^-'); hold on;
    axis tight
    xlabel('Deformation (mm)');
    ylabel('Force (kg)');
    title (  ['s2, [', no_step(1,:), '], w=[', num2str(w), ']' ], 'Interpreter', 'none');
    
    % ##############################################
    % ##                                          ##
    % ##                                          ##
    % ##            Original Thickness            ##
    % ##                                          ##
    % ##                                          ##
    % ##############################################
    % ==========================
    % #####################################################
    % ##        Pick 4 points and take a section         ##
    % #####################################################
    subplot(3,1,1);
    % % plot( [0 10] ,[0  10],'r-o')
    %
    %     text (0, 15, 'select section for Original Thickness (2 point)','FontSize', 10, 'Color',[1 0 0]);
    title ('Select points on the original flat portion of the graph for original thickness, two points total.','FontSize', 10, 'Color',[1 0 0]);
    
    
    clear  q   xy_data
    for q=1:2
        if auto_load=='y'
            center_raw=load([ save_file, 's2_xy_data_', no_step(w,:),'.csv']);
            center_data=center_raw(9:10,:);
            center=center_data(q,:);
            x=center(1);
            y=center(2);
        else
            [x, y]=ginput(1);
        end
        plot(x, y, 'bx', 'linewidth',1);hold on;
        xy_data(q,:)=[floor(x), floor(y)];
    end
    xy_data3= xy_data;
    thick_mm=mean(   deform_raw (xy_data(1)  :xy_data(2)    )    );
    %     text (1300, 15, ['[',num2str(thick_mm), ' mm]'],'FontSize', 10, 'Color',[1 0 0]);
    title ( ['Original Thickness [',num2str(thick_mm), ' mm]'],'FontSize', 10, 'Color',[1 0 0]);
    
    
    
    % =============================
    % ==       Save file         ==
    % =============================
    if save_as=='y'
        save_data=[time100, deform100, force100];
        xy_data= [xy_data1;xy_data2; xy_data3;  thick_mm, 0]; % deform 4point; force 4point; thick 4point
        
        csvwrite( [save_file, 's2_xy_data_', no_step(w,:),'.csv'   ], xy_data);
        csvwrite( [save_file, 's2_young_', no_step(w,:),'.csv'  ], save_data);
        currFig1 = get(0,'CurrentFigure');
        saveas (currFig1,[save_file, 's2_young_', no_step(w,:)],'jpg' )
    end
end
