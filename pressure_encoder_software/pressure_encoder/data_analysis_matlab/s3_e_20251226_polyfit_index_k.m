clc; clear all; close all;
name_file='/Volumes/T7/biodesign/projects/oct_pressure_encoder/pressure_encoder_software/pressure_encoder/data_analysis_matlab/';
open_file=	[name_file, 'data_open/'] ;
save_file=	[name_file, 'data_save/'] ;
auto_load='y';
save_as='y';

index_v=0.45; %  "v" is Poisson's ratio
index_a=4.5;  % (unit: mm) "a" is the radius of the indentor
ratio_h3=[0.10, 0.15, 0.20]; % (unit: mm) % of the thickness
%   Zheng, Y. P. and A. F. Mak (1999). "Extraction of quasi-linear viscoelastic parameters for lower limb soft tissues from manual indentation experiment." J Biomech Eng 121(3): 330-339.
% It increased about 5 times from the initial state to the state of 30 percent deformation.

index_k_serial=[
    0.2	1.252
    0.4	1.599
    0.6	2.031
    0.8	2.532
    1	3.085
    1.5	4.638
    2	6.38
    2.5	8.265
    3	10.26
    3.5	12.32
    4	14.45
    5	18.8
    6	23.23
    7	27.69
    8	32.15];



%% ===================
% ========================
% ===     Filename      ==
% ========================
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
% ===========================

clear w
% for w=1: length(no_step(:,1))
for w=1
    close all
    clear m1 m2 m3 m4
    xy_data=load( [save_file 's2_xy_data_', no_step(w,:),'.csv']  );
    young_data=load( [save_file 's2_young_', no_step(w,:),'.csv']  );
    index_h=xy_data(11,1) ;  % (unit: mm) "h" is the tissue thickness
    index_ah=index_a/index_h;
    % =========================================
    % ==   Split time (sec) into 101 points  ==
    % =========================================
    clear x y xi yi
    y=index_k_serial(:,2);
    x=index_k_serial(:,1);
    xi=index_ah;
    % ==========================
    %         yi=interp1(x,y,xi,'linear');
    % ==========================
    [p1,s1] = polyfit(x,y,2);
    yi = polyval(p1,xi, s1);
    % ==========================
    index_k=yi;
    data_w=young_data( : , 2 ); % check the PIXEL convert mm
    data_f=young_data( : , 3 );   % check the VOLT convert Kg
    % ==========================
    % ==========================
    
    % % %     % ======================================
    % % %     % ==    Polyfit_cen  LOAD -- UNLOAD   ==
    % % %     % ======================================
    % % %       clear  i  a b c  data_f_load data_f_unload
    % % %     a=data_f;
    % % %     for i=1:length(a)-1
    % % %         if a(i+1)>a(i)
    % % %             b(i)=1;
    % % %         else b(i)=0;
    % % %         end
    % % %     end
    % % %     no_load= find (b==1); no_unlo= find (b==0);
    % % %     data_w_load=data_w(no_load); data_w_unlo=data_w(no_unlo);
    % % %     data_f_load=data_f(no_load);    data_f_unlo=data_f(no_unlo);
    % % %     % ============================
    % % %     % ==    Ployfit_cen  LOAD   ==
    % % %     % ============================
    % % %     clear x y
    % % %     x=data_w_load;
    % % %     y=data_f_load;
    % % %     p2=polyfit(x,y,2);
    % % %     xi=0: max(x)/100:  max(x);
    % % %     f2=polyval(p2,xi);
    % % %     plot(x,y,'go',xi,f2,'g-' ,'linewidth',3 ); hold on;
    % % %     % ===============================
    % % %     % ==    Ployfit_cen  UN-LOAD   ==
    % % %     % ===============================
    % % %     clear x y
    % % %     x=data_w_unlo;
    % % %     y=data_f_unlo;
    % % %     p2=polyfit(x,y,2);
    % % %     xi=0: max(x)/100:  max(x);
    % % %     f2=polyval(p2,xi);
    % % %     plot(x,y,'bo',xi,f2,'b-', 'linewidth',3 ); hold on;
    % =======================
    % ==    Ployfit_cen    ==
    % =======================
    clear x y p2
    x=data_w;
    y=data_f;
    [p2, s2]=polyfit(x,y,2);
    xi=0: max(x)/100:  max(x);
    f2=polyval(p2,xi, s2);
    plot(x,y,'ro',xi,f2,'r-', 'linewidth',2  ); hold on;
    % ==========================
    
    e_function=(    (1-(index_v^2))  /  (2*index_a*index_k)   )   *9813.43; % check the XXX convert kPa
        %     1kg/cm^2= 98.1343 kPa;      1kg/mm^2= 9813.43 kPa
    for i=1:3
        
        ed3=index_h*ratio_h3(i);
        ef3=polyval(p2,ed3);
        plot(ed3,ef3,'g^', 'linewidth',4  ); hold on;
        
        if  ed3>max(data_w)
            ed3i=max(data_w);
        else ed3i=ed3;
        end
        
        ef3i=polyval(p2,ed3i);
        ee3i=ef3i/ed3i*e_function;
        plot(ed3i,ef3i,'bsquare', 'linewidth',4  ); hold on;
        text (ed3i,ef3i+0.001, ['E', num2str(i), '=', num2str(roundn (ee3i,-1)), ' kPa'], ...
            'HorizontalAlignment','center', 'Color',[0, 0 ,1], 'FontSize', 15);
        
        e_young3(i)=ee3i;
        e_force3(i)=ef3i;
        e_deform3(i)=ed3i;
    end

    e_young23=( e_force3(3)-e_force3(2) )/  ( e_deform3(3) - e_deform3(2) )  *e_function;
    text (ed3i,ef3i+0.002, ['E23=', num2str(roundn (e_young23,-1)), ' kPa'],...
        'HorizontalAlignment','center', 'Color', 'm', 'FontSize', 15);
    
    
%             title (  ['s3, [', no_step(1,:), '], w=[', num2str(w), '], E=[  ', num2str(roundn(e_30,-1)), '  kPa], ' ]);
        title (  ['s3, [', no_step(1,:), '], w=[', num2str(w), '], Thickness=[',num2str(index_h), ' mm]'], 'Interpreter', 'none');
    %
        axis tight
        xlabel('Deformation (mm)');
        ylabel('Force (kg)');
        %axis([0 4.5 0 1.4] );
        axis tight;
        % ===========================
        % ==      Save file        ==
        % ===========================
        savedata=[w,  e_young3, e_young23,  e_force3,  e_deform3, index_h, index_ah, index_k ]; %1+3+1+3+3+1+1+1= 14 data
        savedata_no_step(w,: )=savedata;
        if save_as=='y'
            csvwrite( [save_file, 's3_savedata_', no_step(w,:),'.csv'   ],savedata);
            currFig1 = get(0,'CurrentFigure');
            saveas (currFig1,[save_file, 's3_e3_', no_step(w,:)],'jpg' )
        end
      data_all(w,:)=  savedata;
end
