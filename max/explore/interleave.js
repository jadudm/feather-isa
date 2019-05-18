// Inlets and Outlets
inlets  = 1;
outlets = 1;
bang_counter = 0;

function bang () {
	bang_counter = bang_counter + 1;
	post("Banged: " + bang_counter);
}	

function msg_int (n) {
	outlet(0, n);
	outlet(0, 1);
}