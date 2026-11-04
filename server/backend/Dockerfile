FROM node:22.11-alpine

WORKDIR /app

# Prvo samo zavisnosti, da Docker može da ih kešira između izmena koda
COPY package.json ./
RUN npm install --omit=dev

COPY src ./src

CMD ["node", "src/index.js"]
