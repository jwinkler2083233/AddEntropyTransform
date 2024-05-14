#include <iostream>
#include <fstream>
#include <bitset>
#include <vector>

#include <Windows.h>

using namespace std;

class BitWriter {
	ofstream output;
	char buffer;
	int buffer_pos;

public:
	BitWriter(const char* filename) :
		output(filename, ios::binary)
		, buffer(0)
		, buffer_pos(0)
	{
	}

	void write_bit(bool bit) {
		buffer |= (bit << (7 - buffer_pos));
		++buffer_pos;
		if (buffer_pos == 8) {
			output.write(&buffer, 1);
			buffer = 0;
			buffer_pos = 0;
		}
	}

	void flush() {
		if (buffer_pos > 0) {
			output.write(&buffer, 1);
			buffer = 0;
			buffer_pos = 0;
		}
	}
};

class BitReader {
	ifstream input;
	char buffer;
	int buffer_pos;

public:
	BitReader(const char* filename) :
		input(filename, ios::binary)
		, buffer(0)
		, buffer_pos(8)
	{
	}

	bool read_bit(bool& bit) {
		if (buffer_pos == 8) {
			if (!read_char(buffer))
				return false; // error
			buffer_pos = 0;
		}
		bit = (buffer & (1 << (7 - buffer_pos))) != 0;
		++buffer_pos;
		return true;
	}

	bool read_char(char& retval) {
		static char buf[1024];
		static int64_t pos = 0;
		static int64_t size = 0;

		if (pos >= size)
		{
			if (!input.read(buf, 1024))
			{
			}
			size = input.gcount();

			if (size == 0)
				return false;
			pos = 0;
		}

		retval = buf[pos++];
		return true;
	}

	void close()
	{
		input.close();
	}
};

class RecentBitsQueue
{
	bool recent1, recent2, recent3, recent4, recent5;
public:
	RecentBitsQueue()
		: recent1(false)
		, recent2(false)
		, recent3(false)
		, recent4(false)
		, recent5(false)
	{}

	void push_back(bool bit)
	{
		recent5 = recent4;
		recent4 = recent3;
		recent3 = recent2;
		recent2 = recent1;
		recent1 = bit;
	}

	bool getMostRecent() {
		return recent1;
	}

	bool get5() {
		return recent5;
	}

	bool get4() {
		return recent4;
	}

	bool isRepeatingTwo() {
		return recent1 == recent2;
	}

	bool isRepeatingThree() {
		return isRepeatingTwo() && recent2 == recent3;
	}

	bool isRepeatingFour() {
		return isRepeatingThree() && recent4 == recent3;
	}

	bool isRepeatingFive() {
		return isRepeatingFour() && recent5 == recent4;
	}
};

class FileStats
{
	int64_t total, zerobits, repeatingTwo, repeatingThree, repeatingFour, repeatingFive;
public:
	RecentBitsQueue queue;

	FileStats()
		: total(0)
		, zerobits(0)
		, repeatingTwo(0)
		, repeatingThree(0)
		, repeatingFour(0)
		, repeatingFive(0)
	{
	}

	void pushBit(bool bit)
	{
		bool ignore = queue.isRepeatingFive();

		queue.push_back(bit);

		// ignore if there is already a streak of over 4 bits
		if (!ignore) {
			if (queue.isRepeatingFive())
			{
				repeatingFour--;
				repeatingFive++;
			}
			else if (queue.isRepeatingFour())
			{
				repeatingThree--;
				repeatingFour++;
			}
			else if (queue.isRepeatingThree())
			{
				repeatingTwo--;
				repeatingThree++;
			}
			else if (queue.isRepeatingTwo())
			{
				repeatingTwo++;
			}
		}

		total++;
		zerobits += (!bit);
	}

	void displayAggregateStats()
	{
		cout << "Total: " << total << endl;
		cout << "Zero bits: " << zerobits << endl;
		cout << "Repeating 2 : " << repeatingTwo << endl;
		cout << "Repeating 3 : " << repeatingThree << endl;
		cout << "Repeating 4 : " << repeatingFour << endl;
		cout << "Repeating 5 : " << repeatingFive << endl;
	}

	void getTotalAndZeroes(int64_t& totl, int64_t& zeros)
	{
		totl = total;
		zeros = zerobits;
	}
};

class FollowerPredictor
{
public:
	FileStats stats;

	FileStats predictionStats, correctPredictionStats;

	FollowerPredictor() {
	}

	bool reverseUsingPrediction(bool bit)
	{
		// args:
		//	bit - the prediction bit
		// return value:
		//	the original value for this bit

		bool prediction = false;

		bool streakOfTwo = stats.queue.isRepeatingTwo();
		bool streakOfThree = stats.queue.isRepeatingThree();

		if (streakOfThree)
		{
			prediction = !stats.queue.getMostRecent();
		}

		if (!streakOfTwo)
		{
			prediction = stats.queue.getMostRecent();
		}

		bool retval = false;

		if (bit)
		{
			// this means that the prediction value was true
			retval = prediction;

			// update stats accordingly
			stats.queue.push_back(prediction);
		}
		else
		{
			// the prediction value didn't match
			retval = !prediction;

			stats.queue.push_back(!prediction);
		}

		return retval;
	}

	bool predictUsingPreviousBit(bool bit)
	{
		// args:
		//	bit - the current bit
		// return value:
		//  true if the prediction is correct, false otherwise
		// 
		// predict using statistics, and then compare to the current bit

		// this is the blind prediction
		bool prediction = false;

		static bool toggle = false;

		bool streakOfTwo = stats.queue.isRepeatingTwo();
		bool streakOfThree = stats.queue.isRepeatingThree();


		if (streakOfThree)
		{
			prediction = !stats.queue.getMostRecent();
		}

		if (!streakOfTwo)
		{
			prediction = stats.queue.getMostRecent();
		}

		// update stats
		stats.pushBit(bit);

		predictionStats.pushBit(prediction);

		correctPredictionStats.pushBit(prediction == bit);
		return prediction == bit;
	}
};

int main()
{
	static const char* inputfile = "c:\\temp\\data.zip";
	static const char* outputfile = "c:\\temp\\data.transform";
	static const char* untransform = "c:\\temp\\data.untransform";

	bool bit;

	::DeleteFileA(untransform);

	FollowerPredictor predictor;

	{
		BitReader reader(inputfile);
		::DeleteFileA(outputfile);
		BitWriter writer(outputfile);

		// read the first bit, and handle it as an initialization
		if (reader.read_bit(bit))
		{
			writer.write_bit(bit);

			// initialize the stats queue with the first 8 bits
			for (int i = 0; i < 8; i++)
			{
				reader.read_bit(bit);
				writer.write_bit(bit);
				predictor.stats.pushBit(bit);
			}

			while (reader.read_bit(bit))
			{
				bool prediction = predictor.predictUsingPreviousBit(bit);

				writer.write_bit(prediction);
			}

			writer.flush();
		}

		predictor.stats.displayAggregateStats();

		cout << "\n\nprediction stats\n" << endl;
		predictor.predictionStats.displayAggregateStats();

		cout << "\n\nCorrect Prediction Stats\n" << endl;
		predictor.correctPredictionStats.displayAggregateStats();
	}

	// read in the transformed file and verify it can be untransformed
	{
		BitReader reader(outputfile);
		BitWriter writer(untransform);

		// read the first bit, and handle it as an initialization
		if (reader.read_bit(bit))
		{
			writer.write_bit(bit);

			// initialize the stats queue with the first 8 bits
			for (int i = 0; i < 8; i++)
			{
				reader.read_bit(bit);
				writer.write_bit(bit);
				predictor.stats.pushBit(bit);
			}

			while (reader.read_bit(bit))
			{
				// reverse the transform
				// 
				// the original call used the previous bits to predict
				bool original = predictor.reverseUsingPrediction(bit);
				writer.write_bit(original);
			}

			writer.flush();
		}
	}
}
