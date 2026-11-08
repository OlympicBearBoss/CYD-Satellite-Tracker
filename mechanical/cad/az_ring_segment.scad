// CYD Satellite Tracker — printable AZ guide-ring segment
// Prototype segment only. Print multiple segments and join on a rigid metal/plywood base.
// Not a bearing race; use rated rollers/bearings and calculate wind loads.
$fn=96;
outer_r=105; inner_r=88; height=14;
segment_angle=60; bolt_d=4.5; bolt_circle_r=97;
module ring_sector(){
  difference(){
    intersection(){
      difference(){
        cylinder(h=height,r=outer_r);
        translate([0,0,-0.1]) cylinder(h=height+0.2,r=inner_r);
      }
      rotate([0,0,-segment_angle/2])
        linear_extrude(height=height)
          polygon(points=[[0,0],[outer_r*1.2,0],[outer_r*1.2*tan(segment_angle),outer_r*1.2],[0,outer_r*1.2]]);
    }
    for(a=[-segment_angle/2+8,segment_angle/2-8])
      rotate([0,0,a]) translate([bolt_circle_r,0,-0.1])
        cylinder(h=height+0.2,d=bolt_d);
  }
}
ring_sector();
