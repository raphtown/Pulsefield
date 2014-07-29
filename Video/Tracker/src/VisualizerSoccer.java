import processing.core.PApplet;
import processing.core.PConstants;
import processing.core.PVector;

class Ball {
	PVector position;  // Position, velocity in normalized (-1,1) coordinate space (maps to entire active area)
	PVector velocity;
	final float deceleration=0.07f*9.81f;  // Decleration while rolling in m/s^2 (mu_r=0.07 from http://www.stmarys-ca.edu/sites/default/files/attachments/files/JGrider.pdf )
	final float restitution=0.7f;   // Coeff of restitution (see http://www.mathematicshed.com/uploads/1/2/5/7/12572836/physicsofkickingsoccerball.pdf )
	final float mass=0.430f;			// Mass of ball in kg (FIFA says 410-450g )
	final float radius=(float)(0.69/2/Math.PI);			// Radius of ball in meters (FIFA say 68-70cm in circumference )
	int inCollision;  // nonzero while in a collision so we don't get multiple hits
	
	public Ball(PVector position, PVector velocity) {
		this.position=position;
		this.velocity=velocity;
		this.inCollision=0;
	}
	
	public void draw(PApplet parent, PVector wsize) {
		final int color=0xff7f007f;
		parent.ellipseMode(PConstants.CENTER);
		float sz=20;
		parent.fill(color,255);
		parent.stroke(color,255);
//		PApplet.println("Ball at "+((position.x+1)*wsize.x/2)+","+((position.y+1)*wsize.y/2));
		PVector p=Tracker.normalizePosition(Tracker.mapPosition(position));
		parent.ellipse((p.x+1)*wsize.x/2, (p.y+1)*wsize.y/2, sz, sz);
	}
	
	public void update(PApplet parent) {
		float elapsed=1.0f/parent.frameRate;
		position.add(PVector.mult(velocity,elapsed));
		if (position.x>Tracker.maxx && velocity.x>0) {
			velocity.x*=-restitution;
			position.x=2*Tracker.maxx-position.x;
		}
		if (position.x<Tracker.minx && velocity.x<0) {
			velocity.x*=-restitution;
			position.x=2*Tracker.minx-position.x;
		}
		if (position.y>Tracker.maxy && velocity.y>0 ) {
			velocity.y*=-restitution;
			position.y=2*Tracker.maxy-position.y;
		}
		if (position.y<Tracker.miny && velocity.y<0) {
			velocity.y*=-restitution;
			position.y=2*Tracker.miny-position.y;
		}
		velocity.mult(1-deceleration*elapsed);
//		PApplet.println("New ball position="+position+", velocity="+velocity+", inCollision="+inCollision);
		inCollision=Math.max(0,inCollision-1);   // Countdown 
	}
	
	// Check for collision with person at position p
	public void collisionDetect(Position p) {
		if (inCollision>0)
			return;
		for (int i=0;i<2;i++) {
			Leg leg=p.legs[i];

			float sep=PVector.dist(leg.getOriginInMeters(),position);
			float minSep=leg.getDiameterInMeters()/2+radius;
			if (sep<minSep && PVector.dot(velocity, leg.getVelocityInMeters())<=0) {
				PApplet.println("Ball at "+position+" with velocity="+velocity+" collided with leg at "+leg.getOriginInMeters()+", velocity="+leg.getVelocityInMeters()+", minsep="+minSep);
				// Assume a kick (but TOOD: if leg is slow, this may be more of a bounce)
				velocity=PVector.mult(leg.getVelocityInMeters(),leg.getMassInKg()/(leg.getMassInKg()+mass)*(1+restitution));
				PApplet.println("New ball velocity="+velocity);
				inCollision=2;
			}
		}
	}
}

public class VisualizerSoccer extends VisualizerDot {
	Ball ball;
	
	VisualizerSoccer(PApplet parent) {
		super(parent);
	}

	@Override
	public void start() {
		super.start();
		// Other initialization when this app becomes active
		ball=new Ball(Tracker.unMapPosition(new PVector(0f,0f)),Tracker.unMapPosition(new PVector(0.1f,0.2f)));
	}
	
	@Override
	public void stop() {
		super.stop();
		// When this app is deactivated
		ball=null;
	}

	@Override
	public void update(PApplet parent, Positions p) {
		// Update internal state
		ball.update(parent);
		for (Position ps: p.positions.values()) {  
			ball.collisionDetect(ps);
		}
	}

	@Override
	public void draw(PApplet parent, Positions p, PVector wsize) {
		super.draw(parent, p, wsize);
		ball.draw(parent,wsize);
	}
	
	@Override
	public void drawLaser(PApplet parent, Positions p) {
//		Laser laser=Laser.getInstance();
//		laser.bgBegin();   // Start a background drawing
//		for (Position ps: p.positions.values()) {  
//			laser.cellBegin(ps.id); // Start a cell-specific drawing
//			Laser drawing code
//			laser.cellEnd(ps.id);
//		}
//		laser.bgEnd();
	}
}