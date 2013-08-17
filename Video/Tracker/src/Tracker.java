import java.awt.Color;
import java.io.IOException;
import java.util.Calendar;
import processing.core.PApplet;
import oscP5.*;
import netP5.*;
import processing.core.PVector;
import promidi.Controller;
import promidi.Note;
import promidi.ProgramChange;

public class Tracker extends PApplet {
	/**
	 * 
	 */
	private static boolean present = false;

	private static final long serialVersionUID = 1L;
	int tick=0;
	private float avgFrameRate=0;
	static OscP5 oscP5;
	NetAddress myRemoteLocation;
	float minx=-3.2f, maxx=3.2f, miny=-3.2f, maxy=3.2f;
	Visualizer vis[];
	VisualizerGrid visAbleton;
	VisualizerNavier visNavier;
	VisualizerDDR visDDR;
	String visnames[]={"Smoke","Navier","Tron","Ableton","DDR","Poly","Voronoi","Guitar","Drums"};
	String vispos[]={"5/1","5/2","5/3","5/4","5/5","4/1","4/2","4/3","4/4"};
	int currentvis=-1;
	static NetAddress TO, MPO, AL, MAX;
	Positions positions;
	Ableton ableton;
	boolean useMAX;
	Synth synth;
	TouchOSC touchOSC;
	int mouseID;
	String configFile;
	URLConfig config;
	AutoCycler cycler;

	public void setup() {
		configFile="/Users/bst/DropBox/Pulsefield/config/urlconfig.txt";

		try {
			config=new URLConfig(configFile);
		} catch (IOException e) {
			System.err.println("Failed to open config: "+configFile+": "+e.getLocalizedMessage());
			System.exit(1);
		}

		size(1280,800, OPENGL);
		frameRate(30);
		mouseID=90;
		cycler=new AutoCycler();
		
		frame.setBackground(new Color(0,0,0));
		positions=new Positions();

		// OSC Setup (but do plugs later so everything is setup for them)
		oscP5 = new OscP5(this, 7002);

		TO = new NetAddress(config.getHost("TO"), config.getPort("TO"));
		MPO = new NetAddress(config.getHost("MPO"), config.getPort("MPO"));
		AL = new NetAddress(config.getHost("AL"), config.getPort("AL"));
		MAX = new NetAddress(config.getHost("MAX"), config.getPort("MAX"));
		ableton = new Ableton(oscP5, AL);
		touchOSC = new TouchOSC(oscP5, TO);
		useMAX=false;

		if (useMAX)
			synth = new Max(this,oscP5, MAX);
		else
			synth = new Synth(this);

		// Visualizers
		vis=new Visualizer[visnames.length];
		vis[0]=new VisualizerPS(this);
		visNavier=new VisualizerNavier(this); vis[1]=visNavier;
		vis[2]=new VisualizerTron(this);
		visAbleton=new VisualizerGrid(this);vis[3]=visAbleton;
		visDDR=new VisualizerDDR(this);vis[4]=visDDR;
		Scale scale=new Scale("Major","C");
		vis[5]=new VisualizerPoly(this,scale,synth);
		vis[6]=new VisualizerVoronoi(this,scale,synth);
		vis[7]=new VisualizerGuitar(this,synth);
		vis[8]=new VisualizerDrums(this,synth);
		setapp(8);

		// Setup OSC handlers
		oscP5.plug(this, "pfframe", "/pf/frame");
		oscP5.plug(this, "pfupdate", "/pf/update");
		oscP5.plug(this, "pfsetnpeople", "/pf/set/npeople");
		oscP5.plug(this, "pfexit", "/pf/exit");
		oscP5.plug(this, "pfentry", "/pf/entry");
		oscP5.plug(this, "pfsetminx", "/pf/set/minx");
		oscP5.plug(this, "pfsetminy", "/pf/set/miny");
		oscP5.plug(this, "pfsetmaxx", "/pf/set/maxx");
		oscP5.plug(this, "pfsetmaxy", "/pf/set/maxy");
		oscP5.plug(this, "pfstarted", "/pf/started");
		oscP5.plug(this, "pfstopped", "/pf/stopped");	
		oscP5.plug(this, "tempo", "/tempo");
		oscP5.plug(this, "ping", "/ping");
	}

	public void tempo(float t) {
		MasterClock.settempo(t);
	}

	public void ping(int code) {
		OscMessage msg = new OscMessage("/ack");
		PApplet.println("Got ping "+code);
		msg.add(code);
		oscP5.send(msg,MPO);
	}

	public void vsetapp(OscMessage msg) {
		for (int i=0;i<vispos.length;i++) {
			if (msg.checkAddrPattern("/video/app/buttons/"+vispos[i]) ) {
				setapp(i);
				return;
			}		
		}
		println("Bad vsetup message: "+msg);
	}

	public static void sendOSC(String dest, OscMessage msg) {
		if (dest.equals("AL"))
			oscP5.send(msg,AL);
		else if (dest.equals("TO"))
			oscP5.send(msg,TO);
		else if (dest.equals("MPO"))
			oscP5.send(msg,MPO);
		else
			System.err.println("sendOSC: Bad destination: "+dest);
	}

	synchronized public void setapp(int appNum) {
		if (appNum <0 || appNum > vis.length) {
			println("Bad video app number: "+appNum);
			return;
		}
		// Turn off old block
		for (int k=0; k<vispos.length;k++)
			if (k!=appNum) {
				OscMessage msg = new OscMessage("/video/app/buttons/"+vispos[k]);
				msg.add(0);
				sendOSC("TO",msg);
				PApplet.println("Sent "+msg.toString());
			}

		if (currentvis!=-1)
			vis[currentvis].stop();
		currentvis=appNum;
		println("Switching to app "+currentvis+": "+visnames[currentvis]);
		// Turn on block for current app
		OscMessage msg = new OscMessage("/video/app/buttons/"+vispos[currentvis]);
		msg.add(1.0);
		sendOSC("TO",msg);

		msg = new OscMessage("/video/app/name");
		msg.add(visnames[currentvis]);
		sendOSC("TO",msg);

		vis[currentvis].start();
	}

	synchronized public void draw() {
		tick++;
		avgFrameRate=avgFrameRate*(1f-1f/200f)+frameRate/200f;
		if (tick%200 == 0) {
			println("Average frame rate = "+avgFrameRate);
			vis[currentvis].stats();
		}

		if (mousePressed) 
			positions.move(mouseID, mouseID%16, new PVector(mouseX*2f/width-1, mouseY*2f/height-1), mouseID, 1, tick/avgFrameRate);


		vis[currentvis].update(this, positions);
		//		translate((width-height)/2f,0);

		vis[currentvis].draw(this,positions,new PVector(width,height));
	}

	public void mouseReleased() {
		//pfexit(0, 0, 98);
		mouseID=(mouseID-90+1)%8+90;
	}

	public static void main(String args[]) {
		if (present)
			PApplet.main(new String[] { "--present","Tracker" });
		else
			PApplet.main(new String[] { "Tracker" });
	}

	/* incoming osc message are forwarded to the oscEvent method. */
	synchronized public void oscEvent(OscMessage theOscMessage) {
		if (theOscMessage.addrPattern().startsWith("/video/app/buttons") == true)
			vsetapp(theOscMessage);
		else if (theOscMessage.addrPattern().startsWith("/grid")) {
			visAbleton.handleMessage(theOscMessage);
		} else if (theOscMessage.addrPattern().startsWith("/live")) {
			ableton.handleMessage(theOscMessage);
		} else if (theOscMessage.addrPattern().startsWith("/video/navier")) {
			visNavier.handleMessage(theOscMessage);
		} else if (theOscMessage.addrPattern().startsWith("/video/ddr")) {
			visDDR.handleMessage(theOscMessage);
		} else if (theOscMessage.addrPattern().startsWith("/midi/pgm")) {
			synth.handleMessage(theOscMessage);
		} else if (theOscMessage.isPlugged() == false) {
			PApplet.print("### Received an unhandled message: ");
			theOscMessage.print();
		}  /* print the address pattern and the typetag of the received OscMessage */
	}

	PVector mapposition(float x, float y) {
		return new PVector((x-minx)/(maxx-minx)*2f-1, (y-miny)/(maxy-miny)*2f-1 );
	}

	synchronized public void pfstarted() {
		PApplet.println("PF started");
	}

	synchronized public void pfstopped() {
		PApplet.println("PF stopped");
		positions.clear();
	}

	void pfframe(int frame) {
		//PApplet.println("Got frame "+frame);
	}

	synchronized void add(int id, int channel) {
		positions.add(id, channel);
	}

	synchronized public void pfupdate(int sampnum, float elapsed, int id, float ypos, float xpos, float yvelocity, float xvelocity, float majoraxis, float minoraxis, int groupid, int groupsize, int channel) {
		/*	if (channel!=99) {
			PApplet.print("update: ");
			PApplet.print("samp="+sampnum);
			PApplet.print(",elapsed="+elapsed);
			PApplet.print(",id="+id);
			PApplet.print(",pos=("+xpos+","+ypos+")");
			PApplet.print(",vel=("+xvelocity+","+yvelocity+")");
			PApplet.print(",axislength=("+majoraxis+","+minoraxis+")");
			PApplet.println(",channel="+channel);
		} */
		ypos=-ypos;
		positions.move(id, channel, mapposition(xpos, ypos), groupid, groupsize, elapsed);
	}

	public void pfsetminx(float minx) {  
		this.minx=minx;
	}
	public void pfsetminy(float miny) {  
		this.miny=miny;
	}
	public void pfsetmaxx(float maxx) {  
		this.maxx=maxx;
	}
	public void pfsetmaxy(float maxy) {  
		this.maxy=maxy;
	}

	synchronized public void pfsetnpeople(int n) {
		PApplet.println("/pf/set/npeople: now have "+n+" people");
		if (n==0 && positions.positions.size()>0) {
			Calendar cal=Calendar.getInstance();
			int hour=cal.get(Calendar.HOUR_OF_DAY);
			cycler.change(hour<7 || hour > 19);
		}
			
		positions.setnpeople(n);
	}

	synchronized public void pfexit(int sampnum, float elapsed, int id) {
		PApplet.println("exit: sampnum="+sampnum+", elapsed="+elapsed+", id="+id);
		positions.exit(id);
	}

	synchronized public void pfentry(int sampnum, float elapsed, int id, int channel) {
		add(id,channel);
		PApplet.println("entry: sampnum="+sampnum+", elapsed="+elapsed+", id="+id+", channel="+channel+", color="+positions.get(id).getcolor(this));
	}

	public void noteOn(Note n, int device, int channel) {
		System.out.println("Got note on: "+n.getPitch()+", vel="+n.getVelocity()+", channel="+channel+", device="+device);
	}

	public void noteOff(Note n, int device, int channel) {
		System.out.println("Got note off: "+n.getPitch()+", vel="+n.getVelocity()+", channel="+channel+", device="+device);
	}

	public void controllerIn(Controller c, int device, int channel) {
		System.out.println("Got controller: "+c.getNumber()+" = "+c.getValue()+", channel="+channel+", device="+device);
	}
	public void programChange(ProgramChange p, int device, int channel) {
		System.out.println("Got program change: "+p.toString()+", channel="+channel+", device="+device);
	}


}

