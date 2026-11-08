// CYD Satellite Tracker — 608 bearing pillow block
// Conceptual PETG/ASA bracket. Verify bearing fit and fastener dimensions before use.
$fn=72;
bearing_od=22.2;       // nominal 608 OD 22 mm + clearance
bearing_width=7.2;
block_w=48; block_h=30; block_d=18;
mount_spacing=32; mount_hole=5.2;
wall=5;
difference(){
  hull(){
    translate([-block_w/2+6,0,0]) cylinder(h=block_d,d=12);
    translate([ block_w/2-6,0,0]) cylinder(h=block_d,d=12);
    translate([0,block_h/2-5,0]) cube([block_w,10,block_d],center=true);
  }
  // Bearing seat, axis along Z
  translate([0,0,block_d-bearing_width+0.01]) cylinder(h=bearing_width+0.2,d=bearing_od);
  translate([0,0,-0.1]) cylinder(h=block_d+0.2,d=10.2);
  for(x=[-mount_spacing/2,mount_spacing/2])
    translate([x,-block_h/2+4,-0.1]) cylinder(h=block_d+0.2,d=mount_hole);
}
// Bearing itself is intentionally not included; use a standard 608 bearing.
