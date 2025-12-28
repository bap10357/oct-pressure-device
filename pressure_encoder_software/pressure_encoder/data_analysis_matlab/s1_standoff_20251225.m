clc; clear all; close all
name_file='/Volumes/T7/biodesign/projects/oct_pressure_encoder/pressure_encoder_software/pressure_encoder/data_analysis_matlab/';
open_file=	[name_file, 'data_open/'] ;
save_file=	[name_file, 'data_save/'] ;
auto_load='n';
save_as='y';
figure_full='n';

%% ===================
% ===========================
% ===     Filename        ===
% ===========================
no1=[1];  % no people
no2=[1];  % MPH: [1] 1.8; [2] 3.6; [3] 5.4;
no3=[1];  % min walking: [1] 10; [2] 20;
no4=[3];  % min of walking: [1] first; [2] last min;
%% ===========================
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

% ===========================
% for w=1: length(no_step(:,1))
for w=1
    % w=1
    close all
    
    clear a1 a2 a3 a4 a index_xii index_yii y1 y2
    close all;
    a1=load(  [open_file,'i', no_step(w,:),'.csv'] );  %read
    %a2=abs(a1).^0.7 ;
    %h = fspecial('average');
    %a3=imfilter(a2,h);
    
    if figure_full=='y'
        set(gcf,'outerposition',get(0,'screensize'));
    end
    image(a1); colorbar; hold on;
    
    % figure;
    % subplot(2,3,1); image(a1); colorbar; hold on;
    % subplot(2,3,2); image(abs(a1)); colorbar; hold on;
    % subplot(2,3,3); image(abs(a1).^0.5); colorbar; hold on;
    % subplot(2,3,4); image(abs(a1).^10); colorbar; hold on;
    % subplot(2,3,5); imagesc(a3); colorbar; hold on;
    
    %   \\\\\\\\\\\\\\\\\\\\\
    %   \\\    Stop BOX   \\\
    %   \\\\\\\\\\\\\\\\\\\\\
    title (['Track left to right, then click stop. [1] Standoff tracking, [2] Deformation tracking  [', no_step(w,:), '], w=[', num2str(w), ']' ], 'Interpreter', 'none');
    size_a=size(a1);
    sx1=round(size_a(2)/10);
    sy1=round(size_a(1)/10);
        
    x_base=[ sx1, sx1,5,5, sx1];
    y_base=[5,sy1,sy1,5,5];
    text_x=[(x_base(1)+x_base(3))  /2  ];
    text_y=[(y_base(1)+y_base(3))  /2  ];
    text_list=['stop'];
    
    plot(x_base, y_base,'color', [0+0.2,1-0.2,0], 'linewidth',3)
    text( text_x ,   text_y  , text_list,...
        'color', [0+0.2,1-0.2,0], 'BackgroundColor',[1 1 1],...
        'HorizontalAlignment', 'center');
    
    %   \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
    %   \\\   Deformation tracking   \\\\\
    %   \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
    %   ==================================
    %   ===        IMPORTANT           ===
    %   === Direction  left to right   ===
    %   ==================================
    color_pick='yo';
    
    clear i j x_i   y_i  tracking_xyi
    
    for j=1:3
        if auto_load=='y'
            xy_track4=load( [ save_file,'s1_xy_track_',no_step(w,:),'.csv'   ]);
        else
            [x_i_raw, y_i_raw]=ginput(1);
            x_i=round(x_i_raw);
            y_i=round(y_i_raw);
            plot( x_i, y_i,               'wo', 'linewidth',3);hold on;
            plot( x_i, y_i,[color_pick], 'linewidth',2);hold on;
            for i=1:100
                [x_i(i+1), y_i(i+1)]=ginput(1);
                if (x_i(i+1)<x_base(2)) &...
                        (x_i(i+1)>x_base(3)) &...
                        (y_i(i+1)>y_base(1)) &...
                        (y_i(i+1)<y_base(2))
                    
                    plot( x_i(i+1), y_i(i+1),[color_pick], 'linewidth',1);hold on;
                    break;
                end
                plot( x_i(i+1), y_i(i+1),'wo'   , 'linewidth',2);hold on;
                plot( x_i(i+1), y_i(i+1),'ro'   , 'linewidth',1);hold on;
                plot( [x_i(i), x_i(i+1)] , [y_i(i), y_i(i+1)],[color_pick(1),'-'], 'linewidth',1);hold on;
            end
            xy_track0=[x_i(1:length(x_i)-1)',y_i(1: length(y_i)-1)'];
            
            length_track0=length(xy_track0(:,1));
            
            xy_track1=[xy_track0(1,1), xy_track0(1,2)];
            xy_track2=[xy_track0(length_track0,1), xy_track0(length_track0,2)];
            xy_track11=[   1, xy_track1(1,2)];
            xy_track22=[   size_a(2), xy_track2(1,2)];
            
            xy_track=[xy_track11;    xy_track0  ;xy_track22];
            
            plot( xy_track(:,1), xy_track(:,2),'wo-', 'linewidth',2);hold on;
            plot( xy_track(:,1), xy_track(:,2),'ro-', 'linewidth',1);hold on;
            
            
            % ======================================================
            % ==    Split line into 201 points   xi_divide=200;   ==
            % ======================================================
            clear xi yi x y bone_x bone_y
            x=xy_track(:,1);
            y=xy_track(:,2);
            xi_divide=200;
            xi=x(1) : ( x(length(x))- x(1) )  /xi_divide : x(length(x));
            yi=interp1(x,y,xi,'linear')';
            xii=round(xi);
            yii=round(yi);
            
            xy_track4( :   ,  j*2-1: j*2 )=[xii', yii];
            
        end
        plot(xy_track4(:, j*2-1), xy_track4(:, j*2), 'w.', 'linewidth', 1);hold on;
    end
    
%     for i=1:2
%         plot(xy_track4(:, i*2-1), xy_track4(:, i*2), 'w-', 'linewidth', 1);hold on;
%     end
    
    soft_x=xy_track4(:,1);
    soft_y12=xy_track4(:,4)-xy_track4(:,2)+xy_track4(1,2);
    soft_y23=xy_track4(:,6)-xy_track4(:,4)+xy_track4(1,4);
    
    plot( soft_x, soft_y12, 'w-', 'linewidth',2)
    plot( soft_x, soft_y12, 'r-', 'linewidth',1)
    plot( soft_x, soft_y23, 'w-', 'linewidth',2)
    plot( soft_x, soft_y23, 'b-', 'linewidth',1)
    
    % ==============================
    % ==       Save file          ==
    % ==============================
    if save_as=='y';
        csvwrite( [ save_file,'s1_xy_track_',no_step(w,:),'.csv'   ], xy_track4);
        % ===========================
        save_data12=[soft_x, soft_y12-xy_track4(1,2)];
        save_data23=[soft_x, soft_y23-xy_track4(1,4)];
        save_data_12_23=[save_data12, save_data23];
        csvwrite( [save_file, 's1_track_',   no_step(w,:),'.csv'   ], save_data_12_23);
        currFig1 = get(0,'CurrentFigure');
        saveas       (currFig1,[save_file, 's1_track_',   no_step(w,:)],'jpg');
    end
end
