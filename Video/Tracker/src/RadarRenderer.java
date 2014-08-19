

import ddf.minim.AudioSource;
import processing.core.PApplet;


public class RadarRenderer extends AudioRenderer {
  
  float aura = .25f;
  float orbit = .25f;
  int delay = 2;
  
  int rotations;

  public RadarRenderer(PApplet parent, AudioSource source) {
    rotations =  (int) source.sampleRate() / source.bufferSize();
  }
  
  @Override
  public void start(PApplet parent) {
	  parent.background(0);
  }
  
  @Override
  public synchronized void draw(PApplet parent)
  {
    parent.colorMode(PApplet.RGB, (float)(Math.PI * 2* rotations), 1, 1);
    if(left != null) {
   
      float t = PApplet.map((float)parent.millis(),0f, delay * 1000f, 0f, (float)Math.PI);   
      int n = left.length;
      
      // center 
      float w = (float) (parent.width/2 + Math.cos(t) * parent.width * orbit);
      float h = (float) (parent.height/2 + Math.sin(t) * parent.height * orbit); 
      
      // size of the aura
      float w2 = parent.width * aura, h2 = parent.height * aura;
      
      // smoke effect
      if(parent.frameCount % delay == 0 ) parent.image(parent.get(),-1.5f,-1.5f,(float)( parent.width + 3), (float)(parent.height + 3)); 
      
      // draw polar curve 
      float a1=0, x1=0, y1=0, r2=0, a2=0, x2=0, y2=0; 
      for(int i=0; i <= n; i++)
      {
        a1 = a2; x1 = x2; y1 = y2;
        r2 = left[i % n] ;
        if (right!=null)
        	r2+=right[i%n];
        a2 = PApplet.map((float)i,0f, (float)n, 0f, (float)(Math.PI * 2 * rotations));
        x2 = (float) (w + Math.cos(a2) * r2 * w2);
        y2 = (float) (h + Math.sin(a2) * r2 * h2);
        parent.stroke(a1, 1, 1, 30);
        // strokeWeight(dist(x1,y1,x2,y2) / 4);
        if(i>0) parent.line(x1, y1, x2, y2);
      }
    }
  }
}
