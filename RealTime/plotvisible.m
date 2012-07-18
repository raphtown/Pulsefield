function plotvisible(sainfo,vis)
setfig('getvisible');
clf;
for i=1:size(vis.lev,1)
  c=sainfo.camera(i).pixcalib;
  roi=sainfo.camera(i).roi;
  if isfield(vis,'im')
    subplot(length(vis.im),2,i*2-1)
    imshow(vis.im{i});
    axis normal
    hold on;
    title(sprintf('Camera %d',sainfo.camera(i).id));
    pause(0.01);
    for j=1:length(c)
      if ~isnan(vis.v(i,j))
        if vis.v(i,j)
          plot(c(j).pos(1)-roi(1)+1,c(j).pos(2)-roi(3)+1,'og');
        else
          plot(c(j).pos(1)-roi(1)+1,c(j).pos(2)-roi(3)+1,'or');
        end
      end
    end
    subplot(size(vis.lev,1),2,i*2);
  else
    subplot(size(vis.lev,1),1,i);
  end
  lev=double(vis.lev(i,:));
  lev(isnan(vis.v(i,:)))=nan;
  plot(lev,'g');
  hold on;
  % Plot threshold
  plot(sainfo.crosstalk.thresh(i,:),'c');
  plot(sainfo.crosstalk.neighlev(i,:),'m');
  % Plot signals below threshold in red
  lev(vis.v(i,:)==1)=nan;
  plot(lev,'rx');
  ax=axis;
  ax(3)=0;ax(4)=260;
  axis(ax);
  xlabel('LED');
  ylabel('Signal');
  legend('Signal','Threshold','Neighlev');
end
