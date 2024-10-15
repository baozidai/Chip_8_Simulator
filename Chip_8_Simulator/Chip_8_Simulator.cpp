// Chip_8_Simulator.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <array>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <iomanip>

//虚拟机内存类
class Memory
{
public:
	static const std::size_t MEMORY_SIZE = 4096;
	static const std::size_t ROM_START_ADDRESS = 0x200; // decimal 512

	void load_rom(const uint8_t* data, std::size_t length, std::size_t address = ROM_START_ADDRESS) {
		if (address + length > MEMORY_SIZE) {
			throw std::out_of_range("Write operation out of memory bounds");
		}
		for (std::size_t i = 0; i < length; i++) {
			memory[address + i] = data[0 + i];
		}
	}
	std::pair<uint8_t, uint8_t> get(std::size_t address) {
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Read operation out of memory bounds");
		}
		return{ memory[address], memory[address + 1] };
	}
	void set(std::uint16_t address, std::pair<uint8_t, uint8_t> data) {
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Write operation out of memory bounds");
		}
		memory[address] = data.first;
		memory[address + 1] = data.second;
	}
private:
	std::array<uint8_t, MEMORY_SIZE> memory = { 0 };
};

//	虚拟机CPU
class Chip8Simulator {
public:
	Chip8Simulator() {
		// Initialize registers
		for (int i = 0; i < 16; i++) {
			V[i] = 0;
		}
		I = 0;
		SP = -1;
		PC = Memory::ROM_START_ADDRESS;
		delay_timer = 0;
		sound_timer = 0;
		memory = Memory();
	};
	void load_rom(const uint8_t* data, std::size_t length) {
		memory.load_rom(data, length);
		std::cout << "File loaded into memory successfully." << std::endl;
		std::cout <<"First 2 byte is: " << std::hex << static_cast<int>(memory.get(Memory::ROM_START_ADDRESS).first)<<" " << std::hex << static_cast<int>(memory.get(Memory::ROM_START_ADDRESS).second) << std::endl;
	}
	void save_registers() {
		uint16_t address = STACK_ADDRESS + SP * STACK_SIZE;
		memory.set(address, { static_cast<uint8_t>(PC >> 8), static_cast<uint8_t>(PC & 0xFF) });
		memory.set(address + 2, { static_cast<uint8_t>(I >> 8), static_cast<uint8_t>(I & 0xFF) });
		for (int i = 0; i < 16; i++) {
			memory.set(address + 4 + i, { V[i], 0 });
		}
		memory.set(address + 20, { delay_timer, 0 });
		memory.set(address + 21, { sound_timer, 0 });
	}
	void execute(const std::pair<uint8_t, uint8_t>& opcode) {
		uint16_t opcode_value = (opcode.first << 8) | opcode.second;
		uint16_t first_high = opcode_value & 0xF000;
		//Execute machine language subroutine at address NNN
		if (first_high == 0x0000) {
			uint16_t address = opcode_value & 0x0FFF;
			if (address >= 0x200) {
				SP++;
				save_registers();
				PC = address;
				return;
			}
			else if (address == 0x00EE) {
				uint16_t address = STACK_ADDRESS + SP * STACK_SIZE;
				PC = (memory.get(address).first << 8) | memory.get(address).second;
				I = (memory.get(address + 2).first << 8) | memory.get(address + 2).second;
				for (int i = 0; i < 16; i++) {
					V[i] = memory.get(address + 4 + i).first;
				}
				delay_timer = memory.get(address + 20).first;
				sound_timer = memory.get(address + 21).first;
				SP--;
				return;
			}
			else if (address == 0x00E0) {
				//Clear the screen
				for (int i = 0; i < 64 * 32; i++) {
					memory.set(DISPLAY_ADDRESS + i, { 0, 0 });
				}
				return;
			}
			else {
				throw std::runtime_error("Invalid opcode");
				return;
			}
		}
		else if (first_high == 0x1000) {
			//Jump to address NNN
			PC = opcode_value & 0x0FFF;
			return;
		}
		else if (first_high == 0x2000) {
			//Call subroutine at NNN
			uint16_t address = opcode_value & 0x0FFF;
			SP++;
			save_registers();
			PC = address;
			return;
		}
		else if (first_high == 0x3000) {
			//Skip next instruction if Vx = kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			if (V[x] == kk) {
				PC += 2;
			}
			return;
		}
		else if (first_high == 0x4000) {
			//Skip next instruction if Vx != kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			if (V[x] != kk) {
				PC += 2;
			}
			return;
		}
		else if (first_high == 0x5000) {
			//Skip next instruction if Vx = Vy
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t y = (opcode_value & 0x00F0) >> 4;
			if (V[x] == V[y]) {
				PC += 2;
			}
			return;
		}
		else if (first_high == 0x6000) {
			//Set Vx = kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			V[x] = kk;
			return;
		}
		else if (first_high == 0x7000) {
			//Set Vx = Vx + kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			V[x] += kk;
			return;
		}

	}
	void run() {
		while (true) {
			auto opcode = memory.get(PC);
			execute(opcode);
		}
	}
private:
	uint8_t V[16]; // 16 8-bit data registers
	uint16_t I; // Address register, only 12-bits used
	uint16_t SP; // Stack Pointer
	uint16_t PC; // Program Counter
	uint8_t delay_timer;
	uint8_t sound_timer;
	uint16_t DISPLAY_ADDRESS = 0xF00;//显存从这里开始，共64*32个像素
	uint16_t STACK_ADDRESS = 0xEA0;//栈从这里开始，共16个栈
	uint16_t STACK_SIZE = 16+2+2+1+1; //栈的大小，只保存除SP的全部数据寄存器的内容。
	Memory memory; // Memory
};

int test_Chip8Simulator() {
	Chip8Simulator chip8;
	//	读取ROM
	const std::string filename = "test_opcode.ch8";
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "ERROR: Cannot open file " << filename << std::endl;
		return 1;
	}
	file.seekg(0, std::ios::end);
	std::size_t filesize = file.tellg();
	file.seekg(0, std::ios::beg);
	uint8_t buffer[Memory::MEMORY_SIZE];
	file.read(reinterpret_cast<char*>(buffer), filesize);
	chip8.load_rom(buffer, filesize);
	return 0;
}

int main()
{
	//	初始化虚拟机内存空间
	Memory mem;

	//	读取ROM
	const std::string filename = "test_opcode.ch8";
	std::ifstream file(filename, std::ios::binary);
	if (!file) {
		std::cerr << "ERROR: Cannot open file " << filename << std::endl;
		return 1;
	}
	file.seekg(0, std::ios::end);
	std::size_t filesize = file.tellg();
	file.seekg(0, std::ios::beg);

	uint8_t buffer[Memory::MEMORY_SIZE];
	file.read(reinterpret_cast<char*>(buffer), filesize);

	try {
		mem.load_rom(buffer, filesize);
	}
	catch (const std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}

	std::cout << "File loaded into memory successfully." << std::endl;
	//	读取示例
	try
	{
		for (int i = 0; i < Memory::MEMORY_SIZE; i = i + 2) {
			auto data_read = mem.get(i);
			std::cout << "Address: " << std::setw(3) << std::setfill('0') << std::hex << i << " "
				<< std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(data_read.first) << " "
				<< std::setw(2) << std::setfill('0') << static_cast<int>(data_read.second) << std::dec << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
	test_Chip8Simulator();
	return 0;

}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
