class VideoInfo
{
public:

	int width;
	int height;

	int rotation;
	int screengap;

	int size() {
		return width*height;
	}

	int rotatedwidth() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height;
			case 180:
				return width;
			case 270:
				return height;
			default:
				return 0;
		}
	}

	int rotatedheight() {
		switch(rotation) {
			case 0:
				return height;
			case 90:
				return width;
			case 180:
				return height;
			case 270:
				return width;
			default:
				return 0;
		}
	}

	int rotatedwidthgap() {
		switch(rotation) {
			case 0:
				return width;
			case 90:
				return height + screengap;
			case 180:
				return width;
			case 270:
				return height + screengap;
			default:
				return 0;
		}
	}

	int rotatedheightgap() {
		switch(rotation) {
			case 0:
				return height + screengap;
			case 90:
				return width;
			case 180:
				return height + screengap;
			case 270:
				return width;
			default:
				return 0;
		}
	}
};
