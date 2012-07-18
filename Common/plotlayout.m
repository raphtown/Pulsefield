function plotlayout(l,newfig)
if nargin<2
  newfig=1;
end
if newfig
  setfig('plotlayout');
  clf;
end
hold on;

title(sprintf('Layout with %d cameras, %d LEDs',size(l.cpos,1),size(l.lpos,1)));
for i=1:size(l.lpos,1)
  h(1)=plot(l.lpos(:,1),l.lpos(:,2),'xm');
end
for k=1:50:size(l.lpos,1)
  text(l.lpos(k,1),l.lpos(k,2),sprintf('L%d',k));
  plot(l.lpos(k,1),l.lpos(k,2),'xk');
end
text(l.lpos(end,1),l.lpos(end,2),sprintf('L%d',size(l.lpos,1)));

labels{1}='LEDs';
for i=1:size(l.cpos,1)
  h(2)=plot(l.cpos(i,1),l.cpos(i,2),'ko');
  plot(0.1*[0,l.cdir(i,1)]+l.cpos(i,1),0.1*[0,l.cdir(i,2)]+l.cpos(i,2),'r-');
end
text(l.cpos(1,1),l.cpos(1,2),'C1');
text(l.cpos(end,1),l.cpos(end,2),sprintf('C%d',size(l.cpos,1)));
labels{2}='Cameras';

h(3)=plot([l.active(:,1);l.active(1,1)],[l.active(:,2);l.active(1,2)],'c');
labels{3}='Active';

h(4)=plot(l.entry(1),l.entry(2),'r*');
labels{4}='Entry';
legend(h,labels);
axis equal
