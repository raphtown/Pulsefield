if ~exist('oldsnap','var')
  p=struct();
  p.analsysisparams=analysissetup();
  p.lidar=struct('id',1);
  [~,myport]=getsubsysaddr('MPV');
  fprintf('Instructing frontend to use port %d to send us msgs\n', myport);
  oscmsgout('FE','/vis/dest/add/port',{myport});
  ok=oscping('FE','MPV');
  if ~felidarctl(p,'ping') && ~felidarctl(p,'start')
    error('Failed to start front end');
  end

  if ~ok
    error('Failed ping of front end');
  end

  oscmsgout('FE','/vis/get/reflect',{uint32(0)});
  oscmsgout('FE','/vis/set/echoes',{uint32(1)});
end

tracker=multiObjectTracking();
bg=[];
iswaiting=false;
snap=[];

while true
  if exist('oldsnap','var')
    % Reprocess oldsnap
    if length(snap)>=length(oldsnap)
      break;
    end
    vis=oldsnap(length(snap)+1).vis;
  else
    vis=sickrcvr('debug',0);
  end
    
  if isempty(vis)
    if iswaiting
      fprintf('.');
    else
      fprintf('Waiting for data from frontend.');

      if length(snap)>0
        diagnostic(snap);
      end
      iswaiting=true;
    end
    pause(0.01);
    continue;
  end
  if iswaiting
    fprintf('done\n');
  end
  iswaiting=false;
  vis.range=vis.range(:,1,:);
  if isfield(vis,'reflect')
    vis.reflect=vis.reflect(:,1,:);
  end

  bg=updatebg(bg,vis);
  vis=classify(vis,bg);
  vis=joinlegs(vis);
  vis=calcbboxes(vis);
  if isempty(snap)
    npredict=1;
  else
    npredict=vis.cframe-snap(end).vis.cframe;
  end
  if npredict>1
    fprintf('Skipping ahead %d frames\n', npredict);
  end
  tracker.update(vis.targets.pos,vis.targets.legs,npredict);
  moveintoshadows(tracker,vis);
  snap=[snap,struct('vis',vis,'bg',bg,'tracker',tracker.clone())];
  if length(snap)>1
    sendosc({'VD'},snap(end),snap(end-1));
  else
    sendosc({'VD'},snap(end));
  end
  
  if mod(length(snap),100)==0
    fprintf('%d...',length(snap));
  end
end
