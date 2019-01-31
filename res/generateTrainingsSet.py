import numpy as np
import matplotlib.pyplot as plt


def generate_for_testing():
  outputVector = np.linspace(-5.0, 5.0, 11, dtype=float)

  inputVector = np.zeros(np.size(outputVector))

  for i in range(np.size(outputVector)):
    inputVector[i] = np.log(261.626 * np.power(2.0, outputVector[i]))

  # for i in outputVector:
  #   print(i)

  # for i in inputVector:
  #   print(i)

  plt.plot(outputVector, inputVector, 'o')
  plt.show()

  # print("trainingExampleFloat dataPoint;")
  # for i in range(np.size(outputVector)):
  #   print("dataPoint.input = { %.2ff };" % round(inputVector[i], 2))
  #   print("dataPoint.output = { %.2ff };" % round(outputVector[i], 1))
  #   print("trainingSet.push_back(dataPoint);")


def generate_out_vector():
  outputVector = np.linspace(-5.0, 5.0, 101, dtype=float)

  print("{ ", end='')
  for i in outputVector:
    print("%.1ff, " % i, end='')
  print(" }")


def generate_midi_to_freq():
  outputVector = np.linspace(0, 127, 128, dtype=float)

  inputVector = np.zeros(np.size(outputVector))

  for i in range(np.size(outputVector)):
    inputVector[i] = np.power(2.0, (outputVector[i] - 69.0) / 12.0) * 440.0

  print(inputVector)
  print(outputVector)
  plt.plot(outputVector, inputVector, 'o')
  plt.show()


def main():
  generate_for_testing()


if __name__ == "__main__":
  main()