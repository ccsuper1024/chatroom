# ChatRoom Client Web

This is the React frontend for the ChatRoom application.

## Prerequisites

- Node.js version 16.0 or higher
- npm version 7.0 or higher

## Setup

1.  Install dependencies:
    ```bash
    npm install
    ```

2.  Development server:
    ```bash
    npm run dev
    ```

3.  Build for production:
    ```bash
    npm run build
    ```
    The build artifacts will be stored in the `dist` directory.

## Backend Integration

The frontend is designed to be served by the C++ backend server.
Ensure the backend server is configured to serve the `dist` directory.

API Endpoints used:
- `/login`: POST
- `/register`: POST
- `/users`: GET
- `/messages`: GET
- WebSocket at `ws://host:port`
