"""Read-only comparison of anatomical fits with the user's reference targets."""
import json
import numpy as np
from pathlib import Path
r = json.loads(Path('cadence weapon calibrator reference/latest.json').read_text())
d = json.loads(Path('diagnostics/v158-world-grips/landmarks.json').read_text())
def mat(v): return np.array(v).reshape(4,4,order='F')
def rotation(m):
    u,_,v=np.linalg.svd(m[:3,:3]); return u@v
def fit(a,b):
    a=np.array(a); b=np.array(b); ac=a.mean(0); bc=b.mean(0)
    u,_,v=np.linalg.svd((a-ac).T@(b-bc)); rot=v.T@u.T
    if np.linalg.det(rot)<0: v[-1]*=-1; rot=v.T@u.T
    m=np.eye(4);m[:3,:3]=rot;m[:3,3]=bc-rot@ac
    return m
for c in r['cases']:
    p={n:mat(v) for n,v in d[c['case']].items()}
    t={b['name']:mat(b['global_matrix_column_major']) for b in c['frozen_pose']}
    tw=np.linalg.inv(t['b_RightHand']); sw=np.linalg.inv(p['R Hand'])
    # Reconstruct current hand-local rigid orientation transplant.
    current=np.eye(4); current[:3,:3]=rotation(mat(d['target']['b_RightHand'])).T@rotation(mat(d['body']['R Hand']))
    print(c['case'], 'native joints', [n for n in p if n.startswith('R ')])
    # Match the relative geometry; compare fitted native wrist->target wrist map
    # with the current map and the human-adjusted map.
    for fingers in [['Index','Middle','Ring','Little'],['Thumb','Index','Middle','Ring','Little']]:
      for levels in [[1],[1,2]]:
        src=[];dst=[]
        for f in fingers:
          for l in levels:
            sn='R '+f+str(l);tn='b_RightFinger'+str({'Thumb':0,'Index':1,'Middle':2,'Ring':3,'Little':4}[f])+('' if l==1 else str(l-1))
            if sn in p and tn in t:src.append((sw@p[sn])[:3,3]);dst.append((tw@t[tn])[:3,3])
        m=fit(src,dst)
        delta=m@np.linalg.inv(current)
        human=mat(c['adjusted']['matrix_column_major'])
        print(fingers,levels,'delta',np.round(delta,3).tolist(),'ref err cm/deg',np.linalg.norm(delta[:3,3]-human[:3,3]),np.degrees(np.arccos(np.clip((np.trace(delta[:3,:3].T@human[:3,:3])-1)/2,-1,1))))
        if len(fingers)==5 and levels==[1,2]:
            residual=np.linalg.inv(m)@human@current
            print('native wrist-space surface calibration (column major):',residual.flatten(order='F').tolist())
    print('human delta', np.round(mat(c['adjusted']['matrix_column_major']),3).tolist())
