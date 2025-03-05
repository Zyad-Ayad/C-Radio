# Radio Browser CLI

A command-line application to browse radio stations by country and play streams using `ffplay`. The application fetches server information via DNS lookup, retrieves available countries and their respective radio stations using the Radio Browser API, and allows the user to select and play a station stream.

## Features

- **DNS Lookup:** As advised in the [API.radio-browser.info docs](https://api.radio-browser.info/), it's better to do a DNS-lookup to get the list of available servers.
- **Fetch Countries & Stations:** Uses HTTP requests to fetch country data and station listings from the Radio Browser API.
- **Stream Playback:** Streams the selected station via `curl` piped into `ffplay` with process management and robust error handling.

## Requirements

- **C Compiler:** GCC or another compliant C compiler.
- **libcurl:** For HTTP requests.
- **cJSON:** For JSON parsing.
- **ffplay:** For audio stream playback (usually part of FFmpeg).
- **POSIX Environment:** The code uses UNIX/POSIX specific calls (e.g., `fork`, `killpg`, `waitpid`).

### Install Dependencies on Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libffmpeg-dev ffmpeg
```

## Build Instructions

1. **Clone the repository:**

   ```bash
   git clone https://github.com/yourusername/radio-browser-cli.git
   cd radio-browser-cli
   ```

2. **Compile the code:**

   ```bash
   gcc -o radio_browser main.c cJSON.c -lcurl
   ```

   *Adjust the linker flags if your installation of cJSON or FFmpeg differs.*

## Usage

Run the compiled application from the terminal:

```bash
./radio_browser
```

Follow the on-screen prompts to:

1. Choose a server from the list obtained via DNS lookup.
2. Select a country to view available radio stations.
3. Pick a station to play the stream.
4. Stop playback by entering `0` when prompted.

## Code Structure

- **main.c:** Contains the main application logic, including menu navigation and memory management.
- **Functions:**
  - `dns_lookup`: Performs DNS lookup for server discovery.
  - `fetch_countries` and `fetch_startions`: Retrieve and parse JSON data using `cJSON` from the API.
  - `play_stream` and `stop_playback`: Manage process creation and termination for stream playback with system call error checking.
