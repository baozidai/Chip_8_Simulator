// Chip_8_Simulator.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#define SDL_MAIN_HANDLED
#include "SDL.h"
#include <iostream>
#include <array>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <chrono>
#include <thread>

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
	std::pair<uint8_t, uint8_t> get_pair(std::size_t address) {
		// 通常用来读取指令
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Read operation out of memory bounds");
		}
		return{ memory[address], memory[address + 1] };
	}
	uint8_t get_byte(std::size_t address) {
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Read operation out of memory bounds");
		}
		return memory[address];
	}
	void set(std::uint16_t address, std::pair<uint8_t, uint8_t> data) {
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Write operation out of memory bounds");
		}
		memory[address] = data.first;
		memory[address + 1] = data.second;
	}
	void set_byte(std::uint16_t address, uint8_t data) {
		if (address >= MEMORY_SIZE) {
			throw std::out_of_range("Write operation out of memory bounds");
		}
		memory[address] = data;
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
	void print_screen() {
		for (int i = 0; i < 64 * 32; i++) {
			std::cout << static_cast<int>(memory.get_byte(DISPLAY_ADDRESS + i));
			if (i % 64 == 0) {
				std::cout << std::endl;
			}
		}
	}
	void load_rom(const uint8_t* data, std::size_t length) {
		memory.load_rom(data, length);
		std::cout << "File loaded into memory successfully." << std::endl;
		std::cout << "First 2 byte is: " << std::hex << static_cast<int>(memory.get_byte(Memory::ROM_START_ADDRESS)) << " " << std::hex << static_cast<int>(memory.get_pair(Memory::ROM_START_ADDRESS).second) << std::endl;
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
			uint16_t low_byte = opcode_value & 0x0FFF;
			if (low_byte == 0x00EE) {
				uint16_t address = STACK_ADDRESS + SP * STACK_SIZE;
				PC = (memory.get_pair(address).first << 8) | memory.get_pair(address).second;
				I = (memory.get_pair(address + 2).first << 8) | memory.get_pair(address + 2).second;
				for (int i = 0; i < 16; i++) {
					V[i] = memory.get_byte(address + 4 + i);
				}
				delay_timer = memory.get_byte(address + 20);
				sound_timer = memory.get_byte(address + 21);
				SP--;
				PC += 2;
				return;
			}
			else if (low_byte == 0x00E0) {
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
				PC += 4;
				return;
			}
			PC += 2;
			return;
		}
		else if (first_high == 0x4000) {
			//Skip next instruction if Vx != kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			if (V[x] != kk) {
				PC += 4;
				return;
			}
			PC += 2;
			return;
		}
		else if (first_high == 0x5000) {
			//Skip next instruction if Vx = Vy
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t y = (opcode_value & 0x00F0) >> 4;
			if (V[x] == V[y]) {
				PC += 4;
				return;
			}
			PC += 2;
			return;
		}
		else if (first_high == 0x6000) {
			//Set Vx = kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			V[x] = kk;
			PC += 2;
			return;
		}
		else if (first_high == 0x7000) {
			//Set Vx = Vx + kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			V[x] += kk;
			PC += 2;
			return;
		}
		else if (first_high == 0x8000) {
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t y = (opcode_value & 0x00F0) >> 4;
			uint8_t last_nibble = opcode_value & 0x000F;
			if (last_nibble == 0x0) {
				//Set Vx = Vy
				V[x] = V[y];
			}
			else if (last_nibble == 0x1) {
				//Set Vx = Vx OR Vy
				V[x] |= V[y];
			}
			else if (last_nibble == 0x2) {
				//Set Vx = Vx AND Vy
				V[x] &= V[y];
			}
			else if (last_nibble == 0x3) {
				//Set Vx = Vx XOR Vy
				V[x] ^= V[y];
			}
			else if (last_nibble == 0x4) {
				//Set Vx = Vx + Vy, set VF = carry
				uint16_t sum = V[x] + V[y];
				V[0xF] = sum > 0xFF ? 1 : 0;
				V[x] = sum & 0xFF;
			}
			else if (last_nibble == 0x5) {
				//Set Vx = Vx - Vy, set VF = NOT borrow
				V[0xF] = V[x] > V[y] ? 1 : 0;
				V[x] -= V[y];
			}
			else if (last_nibble == 0x6) {
				//Set Vx >>= Vy
				V[0xF] = V[x] & 0x1;
				V[x] >>= V[y];
			}
			else if (last_nibble == 0x7) {
				//Set Vx = Vy - Vx, set VF = NOT borrow
				V[0xF] = V[y] > V[x] ? 1 : 0;
				V[x] = V[y] - V[x];
			}
			else if (last_nibble == 0xE) {
				//Set Vx <<= Vy
				V[0xF] = V[x] & 0b10000000;
				V[x] >>= V[y];
			}
			else {
				throw std::runtime_error("Invalid opcode");
			}
			PC += 2;
		}
		else if (first_high == 0x9000) {
			//Skip next instruction if Vx != Vy
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t y = (opcode_value & 0x00F0) >> 4;
			if (V[x] != V[y]) {
				PC += 4;
			}
			else {
				PC += 2;
			}
		}
		else if (first_high == 0xA000) {
			//Set I = nnn
			I = opcode_value & 0x0FFF;
			PC += 2;
		}
		else if (first_high == 0xB000) {
			//Jump to location nnn + V0
			PC = (opcode_value & 0x0FFF) + V[0];
		}
		else if (first_high == 0xC000) {
			//Set Vx = random byte AND kk
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t kk = opcode_value & 0x00FF;
			V[x] = (rand() % 256) & kk;
			PC += 2;
		}
		else if (first_high == 0xD000) {
			// Display/draw sprite
			uint8_t x = V[(opcode_value & 0x0F00) >> 8];
			uint8_t y = V[(opcode_value & 0x00F0) >> 4];
			uint8_t height = opcode_value & 0x000F;
			uint8_t sprite_byte;

			V[0xF] = 0;
			for (int yline = 0; yline < height; yline++) {
				sprite_byte = memory.get_byte(I + yline);
				for (int xline = 0; xline < 8; xline++) {
					uint8_t pixel = sprite_byte & (0x80 >> xline);
					uint16_t pixel_byte_addr = DISPLAY_ADDRESS + (x + xline) / 8 + ((y + yline) * 8);
					if (pixel != 0) {
						if (pixel & (memory.get_byte(pixel_byte_addr) & (0x80 >> (x + xline) % 8))) {
							V[0xF] = 1;
						}
						pixel = pixel << xline;
						memory.set_byte(pixel_byte_addr, memory.get_byte(pixel_byte_addr) ^ (pixel >> ((x + xline) % 8)));
					}
				}
			}
			PC += 2;
		}
		else if (first_high == 0xE000) {
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t last_byte = opcode_value & 0x00FF;
			if (last_byte == 0x9E) {
				//Skip next instruction if key with the value of Vx is pressed
				PC += 4;
			}
			else if (last_byte == 0xA1) {
				//Skip next instruction if key with the value of Vx is not pressed
				PC += 4;
			}
			else {
				throw std::runtime_error("Invalid opcode");
			}
			PC += 2;
		}
		else if (first_high == 0xF000) {
			uint8_t x = (opcode_value & 0x0F00) >> 8;
			uint8_t last_byte = opcode_value & 0x00FF;
			if (last_byte == 0x07) {
				//Set Vx = delay timer value
				V[x] = delay_timer;
			}
			else if (last_byte == 0x0A) {
				//Wait for a key press, store the value of the key in Vx
			}
			else if (last_byte == 0x15) {
				//Set delay timer = Vx
				delay_timer = V[x];
			}
			else if (last_byte == 0x18) {
				//Set sound timer = Vx
				sound_timer = V[x];
			}
			else if (last_byte == 0x1E) {
				//Set I = I + Vx
				I += V[x];
			}
			else if (last_byte == 0x29) {
				//Set I = location of sprite for digit Vx
				I = V[x] * 5;
			}
			else if (last_byte == 0x33) {
				//Store BCD representation of Vx in memory locations I, I+1, and I+2
				uint8_t hundreds = V[x] / 100;
				uint8_t tens = (V[x] / 10) % 10;
				uint8_t ones = V[x] % 10;
				memory.set(I, { hundreds, 0 });
				memory.set(I + 1, { tens, 0 });
				memory.set(I + 2, { ones, 0 });
			}
			else if (last_byte == 0x55) {
				//Store registers V0 through Vx in memory starting at location I
				for (int i = 0; i <= x; i++) {
					memory.set(I + i, { V[i], 0 });
				}
			}
			else if (last_byte == 0x65) {
				//Read registers V0 through Vx from memory starting at location I
				for (int i = 0; i <= x; i++) {
					V[i] = memory.get_byte(I + i);
				}
			}
			else {
				throw std::runtime_error("Invalid opcode");

			}
			PC += 2;
		}
	}
	void print_memory_pixels() {
		const std::size_t start_address = 3840;
		const std::size_t length = 256;
		const std::size_t pixels_per_byte = 8;
		const std::size_t bytes_per_line = 8;

		for (std::size_t i = 0; i < length; i++) {
			auto byte = memory.get_byte(start_address + i);
			for (std::size_t bit = 0; bit < pixels_per_byte; bit++) {
				std::cout << ((byte & (0x80 >> bit)) ? "█" : " ");
			}
			if ((i + 1) % bytes_per_line == 0) {
				std::cout << std::endl;
			}
		}
	}
	int run() {
		while (true) {
			auto opcode = memory.get_pair(PC);
			execute(opcode);
		}
	}
	Memory& step() {
		auto opcode = memory.get_pair(PC);
		std::cout.width(3); std::cout.fill('0');
		std::cout << "PC: " << std::hex << PC;
		std::cout << " opcode: " << std::setw(2) << std::setfill('0') << std::hex << std::hex << static_cast<int>(opcode.first);
		std::cout << " " << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(opcode.second) << std::endl;
		execute(opcode);
		return memory;
	}
	void print_status() const {
		std::cout << "I: " <<std::setw(3)<<std::setfill('0') << std::hex << I << " ";
		std::cout << "SP: " << std::setw(3) << std::setfill('0') << std::hex << SP << " ";
		std::cout << "PC: " << std::hex << PC << " ";
		//std::cout << "Delay Timer: " << std::hex << static_cast<int>(delay_timer) << " ";
		//std::cout << "Sound Timer: " << std::hex << static_cast<int>(sound_timer) << " ";
		for (size_t i = 0; i < 16; i++)
		{
			std::cout << "V["<<std::hex<<i<<"]: "<< std::hex << static_cast<int>(V[i]) << ", ";
		}
		std::cout << std::endl << std::endl;
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
	uint16_t STACK_SIZE = 16 + 2 + 2 + 1 + 1; //栈的大小，只保存除SP的全部数据寄存器的内容。
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
	chip8.run();
	return 0;
}

const int SCREEN_WIDTH = 64;
const int SCREEN_HEIGHT = 32;
const int PIXEL_SIZE = 10;

void render_screen(SDL_Renderer* renderer, Memory& memory) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // 黑色背景
	SDL_RenderClear(renderer); // 清除屏幕

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // 白色像素

	// 遍历显存（64x32 = 2048位，等于256字节）
	for (int y = 0; y < SCREEN_HEIGHT; ++y) {
		for (int x_byte = 0; x_byte < SCREEN_WIDTH / 8; ++x_byte) {
			uint8_t byte = memory.get_byte(0xF00 + y * (SCREEN_WIDTH / 8) + x_byte);

			for (int bit = 0; bit < 8; ++bit) {
				if (byte & (0x80 >> bit)) { // 检查当前位是否为1
					int x = (x_byte * 8 + bit) * PIXEL_SIZE;
					int y_pos = y * PIXEL_SIZE;
					SDL_Rect rect = { x, y_pos, PIXEL_SIZE, PIXEL_SIZE };
					SDL_RenderFillRect(renderer, &rect); // 绘制像素
				}
			}
		}
	}
	SDL_RenderPresent(renderer); // 更新显示
}

int main(int argc, char* args[])
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("CHIP-8 Emulator", 100, 100, 640, 320, SDL_WINDOW_SHOWN);
	if (!window) {
		std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		SDL_DestroyWindow(window);
		std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}


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


	bool running = true;
	SDL_Event e;
	while (running) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				running = false;
			}
		}
		render_screen(renderer, chip8.step());
		chip8.print_status();
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
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
