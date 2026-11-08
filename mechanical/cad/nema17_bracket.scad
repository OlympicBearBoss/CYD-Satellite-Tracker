// CYD Satellite Tracker — NEMA17 motor mounting plate
// Check motor pilot, screw spacing and shaft clearance against the exact motor.
$fn=64;
plate_w=70; plate_h=70; plate_t=6;
motor_spacing=31; screw_d=3.6; center_clearance=24;
mount_spacing=54; mount_d=5.2;
difference(){
  linear_extrude(height=plate_t)
    offset(r=4) square([plate_w-8,plate_h-8],center=true);
  // NEMA17 31 mm square hole pattern
  for(x=[-motor_spacing/2,motor_spacing/2], y=[-motor_spacing/2,motor_spacing/2])
    translate([x,y,-0.1]) cylinder(h=plate_t+0.2,d=screw_d);
  translate([0,0,-0.1]) cylinder(h=plate_t+0.2,d=center_clearance);
  // Frame mounting holes
  for(x=[-mount_spacing/2,mount_spacing/2])
    translate([x,0,-0.1]) cylinder(h=plate_t+0.2,d=mount_d);
}
